// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/filename_util.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

class RenderViewHostTestBrowserClient : public TestContentBrowserClient {
 public:
  RenderViewHostTestBrowserClient() {}

  RenderViewHostTestBrowserClient(const RenderViewHostTestBrowserClient&) =
      delete;
  RenderViewHostTestBrowserClient& operator=(
      const RenderViewHostTestBrowserClient&) = delete;

  ~RenderViewHostTestBrowserClient() override {}

  bool IsHandledURL(const GURL& url) override {
    return url.scheme() == url::kFileScheme;
  }
};

class RenderViewHostTest : public RenderViewHostImplTestHarness {
 public:
  RenderViewHostTest() : old_browser_client_(nullptr) {}

  RenderViewHostTest(const RenderViewHostTest&) = delete;
  RenderViewHostTest& operator=(const RenderViewHostTest&) = delete;

  ~RenderViewHostTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    old_browser_client_ = SetBrowserClientForTesting(&test_browser_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

 private:
  RenderViewHostTestBrowserClient test_browser_client_;
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

class MockDraggingRenderViewHostDelegateView
    : public RenderViewHostDelegateView {
 public:
  ~MockDraggingRenderViewHostDelegateView() override {}
  void StartDragging(const DropData& drop_data,
                     const url::Origin& source_origin,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& cursor_offset,
                     const gfx::Rect& drag_obj_rect,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override {
    drag_url_ = drop_data.url;
    html_base_url_ = drop_data.html_base_url;
  }

  GURL drag_url() {
    return drag_url_;
  }

  GURL html_base_url() {
    return html_base_url_;
  }

 private:
  GURL drag_url_;
  GURL html_base_url_;
};

TEST_F(RenderViewHostTest, StartDragging) {
  TestWebContents* web_contents = contents();
  MockDraggingRenderViewHostDelegateView delegate_view;
  web_contents->set_delegate_view(&delegate_view);

  DropData drop_data;
  // If `html` is not populated, `html_base_url` won't be populated when
  // converting to `DragData` with `DropDataToDragData`.
  drop_data.html = std::u16string();

  GURL blocked_url = GURL(kBlockedURL);
  GURL file_url = GURL("file:///home/user/secrets.txt");
  drop_data.url = file_url;
  drop_data.html_base_url = file_url;
  test_rvh()->TestStartDragging(drop_data);
  EXPECT_EQ(blocked_url, delegate_view.drag_url());
  EXPECT_EQ(blocked_url, delegate_view.html_base_url());

  GURL http_url = GURL("http://www.domain.com/index.html");
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  test_rvh()->TestStartDragging(drop_data);
  EXPECT_EQ(http_url, delegate_view.drag_url());
  EXPECT_EQ(http_url, delegate_view.html_base_url());

  GURL https_url = GURL("https://www.domain.com/index.html");
  drop_data.url = https_url;
  drop_data.html_base_url = https_url;
  test_rvh()->TestStartDragging(drop_data);
  EXPECT_EQ(https_url, delegate_view.drag_url());
  EXPECT_EQ(https_url, delegate_view.html_base_url());

  GURL javascript_url = GURL("javascript:alert('I am a bookmarklet')");
  drop_data.url = javascript_url;
  drop_data.html_base_url = http_url;
  test_rvh()->TestStartDragging(drop_data);
  EXPECT_EQ(javascript_url, delegate_view.drag_url());
  EXPECT_EQ(http_url, delegate_view.html_base_url());
}

TEST_F(RenderViewHostTest, DragEnteredFileURLsStillBlocked) {
  DropData dropped_data;
  gfx::PointF client_point;
  gfx::PointF screen_point;
  // We use "//foo/bar" path (rather than "/foo/bar") since dragged paths are
  // expected to be absolute on any platforms.
  base::FilePath highlighted_file_path(FILE_PATH_LITERAL("//tmp/foo.html"));
  base::FilePath dragged_file_path(FILE_PATH_LITERAL("//tmp/image.jpg"));
  base::FilePath sensitive_file_path(FILE_PATH_LITERAL("//etc/passwd"));
  GURL highlighted_file_url = net::FilePathToFileURL(highlighted_file_path);
  GURL dragged_file_url = net::FilePathToFileURL(dragged_file_path);
  GURL sensitive_file_url = net::FilePathToFileURL(sensitive_file_path);
  dropped_data.url = highlighted_file_url;
  dropped_data.filenames.push_back(
      ui::FileInfo(dragged_file_path, base::FilePath()));

  // TODO(paulmeyer): These will need to target the correct specific
  // RenderWidgetHost to work with OOPIFs. See crbug.com/647249.
  rvh()->GetWidget()->FilterDropData(&dropped_data);
  rvh()->GetWidget()->DragTargetDragEnter(
      dropped_data, client_point, screen_point, blink::kDragOperationNone, 0,
      base::DoNothing());

  int id = process()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Permissions are not granted at DragEnter.
  EXPECT_FALSE(policy->CanRequestURL(id, highlighted_file_url));
  EXPECT_FALSE(policy->CanReadFile(id, highlighted_file_path));
  EXPECT_FALSE(policy->CanRequestURL(id, dragged_file_url));
  EXPECT_FALSE(policy->CanReadFile(id, dragged_file_path));
  EXPECT_FALSE(policy->CanRequestURL(id, sensitive_file_url));
  EXPECT_FALSE(policy->CanReadFile(id, sensitive_file_path));
}

TEST_F(RenderViewHostTest, MessageWithBadHistoryItemFiles) {
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file_path));
  file_path = file_path.AppendASCII("foo");
  EXPECT_EQ(0, process()->bad_msg_count());
  test_rvh()->TestOnUpdateStateWithFile(file_path);
  EXPECT_EQ(1, process()->bad_msg_count());

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      process()->GetID(), file_path);
  test_rvh()->TestOnUpdateStateWithFile(file_path);
  EXPECT_EQ(1, process()->bad_msg_count());
}

TEST_F(RenderViewHostTest, NavigationWithBadHistoryItemFiles) {
  GURL url("http://www.google.com");
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &file_path));
  file_path = file_path.AppendASCII("bar");

  EXPECT_EQ(0, process()->bad_msg_count());
  auto navigation1 =
      NavigationSimulatorImpl::CreateRendererInitiated(url, main_test_rfh());
  navigation1->set_page_state(
      blink::PageState::CreateForTesting(url, false, "data", &file_path));
  navigation1->Commit();
  EXPECT_EQ(1, process()->bad_msg_count());

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      process()->GetID(), file_path);
  auto navigation2 =
      NavigationSimulatorImpl::CreateRendererInitiated(url, main_test_rfh());
  navigation2->set_page_state(
      blink::PageState::CreateForTesting(url, false, "data", &file_path));
  navigation2->Commit();
  EXPECT_EQ(1, process()->bad_msg_count());
}

TEST_F(RenderViewHostTest, RoutingIdSane) {
  RenderFrameHostImpl* root_rfh =
      contents()->GetPrimaryFrameTree().root()->current_frame_host();
  EXPECT_EQ(contents()->GetPrimaryMainFrame(), root_rfh);
  EXPECT_EQ(test_rvh()->GetProcess(), root_rfh->GetProcess());
  EXPECT_NE(test_rvh()->GetRoutingID(), root_rfh->GetRoutingID());
}

}  // namespace content
