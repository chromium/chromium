// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_drag_dest_mac.h"

#include <AppKit/AppKit.h>

#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/stack_allocated.h"
#include "base/strings/sys_string_conversions.h"
#import "content/browser/web_contents/web_contents_view_mac.h"
#include "content/public/common/drop_data.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/test/cocoa_helper.h"

class WebDragDestTest : public content::RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostImplTestHarness::SetUp();
    drag_dest_ = [[WebDragDest alloc] initWithWebContentsImpl:contents()];
  }

  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  base::apple::ScopedNSAutoreleasePool pool_;
  WebDragDest* __strong drag_dest_;
};

// Make sure nothing leaks.
TEST_F(WebDragDestTest, Init) {
  EXPECT_TRUE(drag_dest_);
}

TEST_F(WebDragDestTest, EndDragSync) {
  // When there is no pending drop operation, calling `endDrag` should just
  // synchronously call any passed callback. If it didn't, the test would fail
  // from `run_loop` timing out.
  base::RunLoop run_loop;
  [drag_dest_ endDrag:run_loop.QuitClosure()];
  run_loop.Run();
}

TEST_F(WebDragDestTest, EndDragAsync) {
  // When there is a pending drop operation, calling `endDrag` should store the
  // passed callback and call it after `completeDropAsync` resolves.
  [drag_dest_ setDropInProgressForTesting];

  base::RunLoop run_loop;
  [drag_dest_ endDrag:run_loop.QuitClosure()];

  EXPECT_FALSE(run_loop.AnyQuitCalled());
  EXPECT_TRUE([drag_dest_ dropInProgressForTesting]);

  [drag_dest_
      completeDropAsync:std::nullopt
            withContext:content::DropContext(content::DropData(), gfx::PointF(),
                                             gfx::PointF(), 0,
                                             main_test_rfh()
                                                 ->GetRenderWidgetHost()
                                                 ->GetWeakPtr())];

  run_loop.Run();
  EXPECT_FALSE([drag_dest_ dropInProgressForTesting]);
}

TEST_F(WebDragDestTest, Data) {
  scoped_refptr<ui::UniquePasteboard> pboard = new ui::UniquePasteboard;
  NSString* html_string = @"<html><body><b>hi there</b></body></html>";
  NSString* text_string = @"hi there";
  [pboard->get() setString:@"http://www.google.com"
                   forType:NSPasteboardTypeURL];
  [pboard->get() setString:html_string forType:NSPasteboardTypeHTML];
  [pboard->get() setString:text_string forType:NSPasteboardTypeString];

  content::DropData data =
      content::PopulateDropDataFromPasteboard(pboard->get());

  EXPECT_EQ(data.url_infos.front().url.spec(), "http://www.google.com/");
  EXPECT_EQ(base::SysNSStringToUTF16(text_string), data.text);
  EXPECT_EQ(base::SysNSStringToUTF16(html_string), data.html);
}

TEST_F(WebDragDestTest, FilePermissions) {
  // Set up a temporary directory
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Define the path for the new temporary file
  base::FilePath temp_file_path = temp_dir.GetPath().AppendASCII("test_file");

  // Set a custom umask (disables write permission for group and others) and
  // store the original umask
  const mode_t kCustomUmask = 0022;
  mode_t original_umask = umask(kCustomUmask);

  // Create the temporary file
  base::File file(temp_file_path,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());

  // Invoke the function to set file permissions
  content::WebContentsViewMac::SetReadWritePermissionsForFileForTests(file);

  // Restore the original umask
  umask(original_umask);

  // Retrieve and verify the file permissions
  int mode = 0;
  ASSERT_TRUE(base::GetPosixFilePermissions(temp_file_path, &mode));

  // Calculate the expected permissions considering the umask
  int expected_permissions = 0666 & ~kCustomUmask;

  EXPECT_EQ(mode, expected_permissions);
}

// Test that drop data survives draggingExited when drop is about to occur
TEST_F(WebDragDestTest, DropDataSurvivedDraggingExited) {
  // Set up drop data
  content::DropData drop_data;
  drop_data.text = u"test text";
  drop_data.url_infos.push_back(
      ui::ClipboardUrlInfo{GURL("http://example.com"), u"Example"});
  [drag_dest_ setDropData:drop_data];

  // Simulate draggingEntered to initialize state
  remote_cocoa::mojom::DraggingInfo info;
  info.location_in_view = gfx::PointF(10, 10);
  info.location_in_screen = gfx::PointF(100, 100);
  info.operation_mask = NSDragOperationCopy;

  [drag_dest_ draggingEntered:&info];

  // Wait for async hit test callback to complete.
  // RunUntilIdle is appropriate here because we're waiting for an async
  // callback that will be posted to the task queue.
  base::RunLoop().RunUntilIdle();

  // Current drop data should exist
  EXPECT_NE(nullptr, [drag_dest_ currentDropData]);

  // Simulate draggingExited - this clears drop data
  [drag_dest_ draggingExited];

  // Drop data should be cleared after draggingExited
  EXPECT_EQ(nullptr, [drag_dest_ currentDropData]);
}

// Test that endDrag preserves drop data when drop is in progress
TEST_F(WebDragDestTest, EndDragPreservesDropDataDuringDrop) {
  // Set up drop data
  content::DropData drop_data;
  drop_data.text = u"test text";
  [drag_dest_ setDropData:drop_data];

  // Simulate drag enter
  remote_cocoa::mojom::DraggingInfo info;
  info.location_in_view = gfx::PointF(10, 10);
  info.location_in_screen = gfx::PointF(100, 100);
  info.operation_mask = NSDragOperationCopy;
  [drag_dest_ draggingEntered:&info];

  // Set drop in progress (simulating performDragOperation)
  [drag_dest_ setDropInProgressForTesting];

  // Verify drop is in progress
  EXPECT_TRUE([drag_dest_ dropInProgressForTesting]);

  // Call endDrag with drop in progress
  base::RunLoop run_loop;
  [drag_dest_ endDrag:run_loop.QuitClosure()];

  // Closure should NOT have run yet (stored for later)
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  // Drop data should still exist
  EXPECT_NE(nullptr, [drag_dest_ currentDropData]);

  // Complete the drop
  [drag_dest_ completeDropAsync:drop_data
                    withContext:content::DropContext(drop_data, gfx::PointF(),
                                                     gfx::PointF(), 0,
                                                     main_test_rfh()
                                                         ->GetRenderWidgetHost()
                                                         ->GetWeakPtr())];

  // Now closure should run
  run_loop.Run();
  EXPECT_FALSE([drag_dest_ dropInProgressForTesting]);
}

// Test that endDrag clears data immediately when no drop is in progress
TEST_F(WebDragDestTest, EndDragClearsDataWhenNoDropInProgress) {
  // Set up drop data
  content::DropData drop_data;
  drop_data.text = u"test text";
  [drag_dest_ setDropData:drop_data];

  // Simulate drag enter
  remote_cocoa::mojom::DraggingInfo info;
  info.location_in_view = gfx::PointF(10, 10);
  info.location_in_screen = gfx::PointF(100, 100);
  info.operation_mask = NSDragOperationCopy;
  [drag_dest_ draggingEntered:&info];

  // Verify data exists
  EXPECT_NE(nullptr, [drag_dest_ currentDropData]);

  // Call endDrag without drop in progress
  base::RunLoop run_loop;
  [drag_dest_ endDrag:run_loop.QuitClosure()];

  // Closure should run immediately
  run_loop.Run();

  // Drop data should be cleared now
  // Note: currentDropData returns the filtered data, which should be null
  // after endDrag clears it
  EXPECT_EQ(nullptr, [drag_dest_ currentDropData]);
}

// Test the complete drop flow: enter -> update -> exit -> perform -> end
TEST_F(WebDragDestTest, CompleteDropFlow) {
  // Set up drop data
  content::DropData drop_data;
  drop_data.text = u"test text";
  drop_data.url_infos.push_back(
      ui::ClipboardUrlInfo{GURL("http://example.com"), u"Example"});
  [drag_dest_ setDropData:drop_data];

  remote_cocoa::mojom::DraggingInfo info;
  info.location_in_view = gfx::PointF(10, 10);
  info.location_in_screen = gfx::PointF(100, 100);
  info.operation_mask = NSDragOperationCopy;

  // 1. Drag enters
  NSDragOperation op = [drag_dest_ draggingEntered:&info];
  EXPECT_EQ(NSDragOperationCopy, op);

  // Wait for async hit test callback to complete.
  // RunUntilIdle is appropriate here because we're waiting for an async
  // callback that will be posted to the task queue.
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, [drag_dest_ currentDropData]);

  // 2. Drag updates
  info.location_in_view = gfx::PointF(20, 20);
  op = [drag_dest_ draggingUpdated:&info];
  EXPECT_NE(NSDragOperationNone, op);

  // 3. Drag exits - this clears drop data
  [drag_dest_ draggingExited];
  // Data should be cleared after exit
  EXPECT_EQ(nullptr, [drag_dest_ currentDropData]);

  // 4. Set drop data again before perform (simulating real drop flow)
  [drag_dest_ setDropData:drop_data];

  // Perform drop (without delegate = synchronous completion)
  BOOL accepted = [drag_dest_ performDragOperation:&info
                       withWebContentsViewDelegate:nullptr];
  EXPECT_TRUE(accepted);

  // Wait for async hit test to complete
  base::RunLoop().RunUntilIdle();

  // After async callback completes with no delegate, drop completes
  // synchronously so the flag should now be false
  EXPECT_FALSE([drag_dest_ dropInProgressForTesting]);

  // 5. End drag (Mac calls this after drop)
  base::RunLoop run_loop;
  [drag_dest_ endDrag:run_loop.QuitClosure()];

  // Should complete immediately since drop already finished
  run_loop.Run();
  EXPECT_FALSE([drag_dest_ dropInProgressForTesting]);
}

// Test drop without delegate (synchronous path)
TEST_F(WebDragDestTest, DropWithoutDelegate) {
  content::DropData drop_data;
  drop_data.text = u"test text";
  [drag_dest_ setDropData:drop_data];

  remote_cocoa::mojom::DraggingInfo info;
  info.location_in_view = gfx::PointF(10, 10);
  info.location_in_screen = gfx::PointF(100, 100);
  info.operation_mask = NSDragOperationCopy;

  [drag_dest_ draggingEntered:&info];

  // Perform drop without delegate
  BOOL accepted = [drag_dest_ performDragOperation:&info
                       withWebContentsViewDelegate:nullptr];
  EXPECT_TRUE(accepted);

  // Wait for async hit test callback to complete
  base::RunLoop().RunUntilIdle();

  // Since there's no delegate, the drop completes synchronously in the async
  // callback The flag gets cleared in OnDropComplete when there's no delegate
  EXPECT_FALSE([drag_dest_ dropInProgressForTesting]);
}

// Test that resetDragDropState properly cleans up
TEST_F(WebDragDestTest, ResetDragDropState) {
  [drag_dest_ setDropInProgressForTesting];
  EXPECT_TRUE([drag_dest_ dropInProgressForTesting]);

  base::RunLoop run_loop;
  [drag_dest_ endDrag:run_loop.QuitClosure()];

  // Reset should clear flag and run closure
  [drag_dest_ resetDragDropState];

  EXPECT_FALSE([drag_dest_ dropInProgressForTesting]);
  run_loop.Run();
}

// Test multiple rapid drag operations (stale callback scenario)
TEST_F(WebDragDestTest, RapidDragOperations) {
  // First drag
  content::DropData drop_data1;
  drop_data1.text = u"first drag";
  [drag_dest_ setDropData:drop_data1];

  remote_cocoa::mojom::DraggingInfo info1;
  info1.location_in_view = gfx::PointF(10, 10);
  info1.location_in_screen = gfx::PointF(100, 100);
  info1.operation_mask = NSDragOperationCopy;

  [drag_dest_ draggingEntered:&info1];
  EXPECT_NE(nullptr, [drag_dest_ currentDropData]);

  // Drag exits
  [drag_dest_ draggingExited];

  // End first drag
  base::RunLoop run_loop1;
  [drag_dest_ endDrag:run_loop1.QuitClosure()];
  run_loop1.Run();

  // Second drag starts immediately
  content::DropData drop_data2;
  drop_data2.text = u"second drag";
  [drag_dest_ setDropData:drop_data2];

  remote_cocoa::mojom::DraggingInfo info2;
  info2.location_in_view = gfx::PointF(20, 20);
  info2.location_in_screen = gfx::PointF(120, 120);
  info2.operation_mask = NSDragOperationCopy;

  NSDragOperation op = [drag_dest_ draggingEntered:&info2];
  EXPECT_EQ(NSDragOperationCopy, op);

  // Complete second drag
  [drag_dest_ draggingExited];
  base::RunLoop run_loop2;
  [drag_dest_ endDrag:run_loop2.QuitClosure()];
  run_loop2.Run();
}

// Test that drop data persists through the critical timing window
TEST_F(WebDragDestTest, DropDataPersistsThroughCriticalWindow) {
  content::DropData drop_data;
  drop_data.text = u"persistent data";
  drop_data.url_infos.push_back(
      ui::ClipboardUrlInfo{GURL("http://test.com"), u"Test"});
  [drag_dest_ setDropData:drop_data];

  remote_cocoa::mojom::DraggingInfo info;
  info.location_in_view = gfx::PointF(10, 10);
  info.location_in_screen = gfx::PointF(100, 100);
  info.operation_mask = NSDragOperationCopy;

  // Enter
  [drag_dest_ draggingEntered:&info];

  // Wait for async hit test callback to complete.
  // RunUntilIdle is appropriate here because we're waiting for an async
  // callback that will be posted to the task queue.
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, [drag_dest_ currentDropData]);

  // Update
  [drag_dest_ draggingUpdated:&info];

  // Wait for async hit test callback to complete.
  // RunUntilIdle is appropriate here because we're waiting for an async
  // callback that will be posted to the task queue.
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, [drag_dest_ currentDropData]);

  // Exit - this clears drop data
  [drag_dest_ draggingExited];
  // Data should be cleared after exit
  EXPECT_EQ(nullptr, [drag_dest_ currentDropData]);

  // Set drop data again before perform (simulating real drop flow)
  [drag_dest_ setDropData:drop_data];

  // Perform drop
  BOOL accepted = [drag_dest_ performDragOperation:&info
                       withWebContentsViewDelegate:nullptr];
  EXPECT_TRUE(accepted);

  // End drag
  base::RunLoop run_loop;
  [drag_dest_ endDrag:run_loop.QuitClosure()];

  // Complete drop
  [drag_dest_
      completeDropAsync:drop_data
            withContext:content::DropContext(drop_data, gfx::PointF(10, 10),
                                             gfx::PointF(100, 100), 0,
                                             main_test_rfh()
                                                 ->GetRenderWidgetHost()
                                                 ->GetWeakPtr())];

  run_loop.Run();
}
