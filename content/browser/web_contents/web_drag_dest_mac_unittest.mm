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
