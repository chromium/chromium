// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_mac.h"

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
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

TEST_F(WebContentsViewMacTest, DragPromisedFileTo_ImageDrag) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  DropData drop_data;
  drop_data.file_contents = "fake data";

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
    std::string file_content;
    if (base::ReadFileToString(actual_path, &file_content)) {
      return drop_data.file_contents == file_content;
    }
    return false;
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

}  // namespace
}  // namespace content
