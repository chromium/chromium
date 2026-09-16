// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_mac.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/test/cocoa_helper.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

namespace {

class FakeContentBrowserClient : public TestContentBrowserClient {
 public:
  bool IsDragAllowedByPolicy(const ClipboardEndpoint& source,
                             const DropData& drop_data) override {
    was_called_ = true;
    return false;
  }

  bool was_called() const { return was_called_; }

 private:
  bool was_called_ = false;
};

class WebContentsViewMacTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    const char kGoogleUrl[] = "https://google.com/";
    NavigateAndCommit(GURL(kGoogleUrl));
    original_client_ = SetBrowserClientForTesting(&fake_client_);

    view_ = std::make_unique<WebContentsViewMac>(
        static_cast<WebContentsImpl*>(contents()), nullptr);

    view_->CreateView(gfx::NativeView());
  }

  void TearDown() override {
    view_.reset();
    SetBrowserClientForTesting(original_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

  gfx::ImageSkia CreateValidDragImage() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return static_cast<RenderWidgetHostImpl*>(
        main_test_rfh()->GetRenderWidgetHost());
  }

  WebContentsViewMac* view() { return view_.get(); }
  FakeContentBrowserClient& fake_client() { return fake_client_; }

 private:
  FakeContentBrowserClient fake_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  std::unique_ptr<WebContentsViewMac> view_;
};

TEST_F(WebContentsViewMacTest, StartDragging_DisallowedByPolicy) {
  DropData drop_data;
  drop_data.text = u"test data";

  view()->StartDragging(*main_rfh(), drop_data, blink::kDragOperationCopy,
                        CreateValidDragImage(), gfx::Vector2d(), gfx::Rect(),
                        blink::mojom::DragEventSourceInfo());

  EXPECT_TRUE(fake_client().was_called());
}

TEST_F(WebContentsViewMacTest, InitiallyHiddenButPaintingNativeView) {
  WebContents::CreateParams params(browser_context());
  params.initially_hidden_but_painting = true;
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(params));

  EXPECT_TRUE([web_contents->GetNativeView().GetNativeNSView() isHidden]);
}

TEST_F(WebContentsViewMacTest, InitiallyHiddenNativeView) {
  WebContents::CreateParams params(browser_context());
  params.initially_hidden = true;
  std::unique_ptr<TestWebContents> web_contents(
      TestWebContents::Create(params));

  EXPECT_TRUE([web_contents->GetNativeView().GetNativeNSView() isHidden]);
}

TEST_F(WebContentsViewMacTest, DragPromisedFileTo_ImageDrag) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  DropData drop_data;
  drop_data.file_contents =
      base::ToVector(base::byte_span_from_cstring("fake data"));

  base::FilePath target_path = temp_dir.GetPath().AppendASCII("test.png");
  base::FilePath actual_path;

  // The overridden Mojo methods are private, so downcast to the base class to
  // work around that.
  remote_cocoa::mojom::WebContentsNSViewHost* host = view();
  bool result = host->DragPromisedFileTo(process()->GetID(),
                                         main_test_rfh()->GetDocumentToken(),
                                         target_path, drop_data, &actual_path);

  EXPECT_TRUE(result);
  EXPECT_EQ(target_path, actual_path);

  // The actual file contents are written out by a task posted to the thread
  // pool.
  ASSERT_TRUE(base::test::RunUntil([&] {
    std::optional<std::vector<uint8_t>> file_content =
        ReadFileToBytes(actual_path);
    if (!file_content) {
      return false;
    }

    return drop_data.file_contents == file_content.value();
  }));
}

TEST_F(WebContentsViewMacTest, DragPromisedFileTo_DownloadURL) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  DropData drop_data;
  drop_data.download_metadata = DownloadUrlMetadata();
  drop_data.download_metadata->url = GURL("https://example.com/file.txt");
  drop_data.download_metadata->mime_type = "text/plain";

  base::FilePath target_path = temp_dir.GetPath().AppendASCII("file.txt");
  base::FilePath actual_path;

  // The overridden Mojo methods are private, so downcast to the base class to
  // work around that.
  remote_cocoa::mojom::WebContentsNSViewHost* host = view();
  bool result = host->DragPromisedFileTo(process()->GetID(),
                                         main_test_rfh()->GetDocumentToken(),
                                         target_path, drop_data, &actual_path);

  EXPECT_TRUE(result);
  EXPECT_EQ(target_path, actual_path);
}

class TestViewsHost : public ui::ViewsHostableView::Host {
 public:
  ui::Layer* GetUiLayer() const override { return nullptr; }
  remote_cocoa::mojom::Application* GetRemoteCocoaApplication() const override {
    return nullptr;
  }
  uint64_t GetNSViewId() const override { return 0; }
  void OnHostableViewDestroying() override {}

  base::ScopedClosureRunner CreateVideoCaptureLock() override {
    ++active_locks_;
    return base::ScopedClosureRunner(base::BindOnce(
        &TestViewsHost::DecrementLocks, weak_factory_.GetWeakPtr()));
  }

  int active_locks() const { return active_locks_; }

 private:
  void DecrementLocks() { --active_locks_; }

  int active_locks_ = 0;
  base::WeakPtrFactory<TestViewsHost> weak_factory_{this};
};

TEST_F(WebContentsViewMacTest, VideoCaptureLockLifecycleAndReparenting) {
  TestViewsHost window_a;
  TestViewsHost window_b;
  auto* mac_view = static_cast<WebContentsViewMac*>(contents()->GetView());

  auto start_capture = [&] {
    return contents()->IncrementCapturerCount(
        gfx::Size(), /*stay_hidden=*/false, /*stay_awake=*/false,
        /*is_activity=*/false);
  };

  // 1. Capturing when unattached to any host does not crash and holds 0 locks.
  base::ScopedClosureRunner capturer_1 = start_capture();
  EXPECT_EQ(window_a.active_locks(), 0);

  // 2. Pre-attachment capture: attaching to Window A while captured
  // automatically acquires the lock on Window A.
  mac_view->ViewsHostableAttach(&window_a);
  EXPECT_EQ(window_a.active_locks(), 1);

  // 3. Deduplication: adding a second concurrent capturer to the same
  // WebContents does not acquire redundant locks on Window A.
  base::ScopedClosureRunner capturer_2 = start_capture();
  EXPECT_EQ(window_a.active_locks(), 1);

  // 4. Partial release: releasing one of two capturers preserves the lock.
  capturer_1.RunAndReset();
  EXPECT_EQ(window_a.active_locks(), 1);

  // 5. Reparenting / Tab drag: dragging the captured tab from Window A to
  // Window B transfers the capture lock from Window A to Window B.
  mac_view->ViewsHostableDetach();
  EXPECT_EQ(window_a.active_locks(), 0);
  EXPECT_EQ(window_b.active_locks(), 0);

  mac_view->ViewsHostableAttach(&window_b);
  EXPECT_EQ(window_a.active_locks(), 0);
  EXPECT_EQ(window_b.active_locks(), 1);

  // 6. Full release: releasing the final capturer releases Window B's lock.
  capturer_2.RunAndReset();
  EXPECT_EQ(window_b.active_locks(), 0);

  // 7. Starting capture while attached to Window B acquires Window B's lock.
  base::ScopedClosureRunner capturer_3 = start_capture();
  EXPECT_EQ(window_b.active_locks(), 1);

  // 8. Detaching while captured releases Window B's lock.
  mac_view->ViewsHostableDetach();
  EXPECT_EQ(window_b.active_locks(), 0);
}

}  // namespace
}  // namespace content
