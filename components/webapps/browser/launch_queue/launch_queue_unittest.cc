// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/launch_queue/launch_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "components/webapps/browser/launch_queue/launch_queue_delegate.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"
#include "url/origin.h"

namespace webapps {

class MockLaunchQueueDelegate : public LaunchQueueDelegate {
 public:
  MOCK_METHOD(bool,
              IsInScope,
              (const LaunchParams& launch_params, const GURL& current_url),
              (const, override));
  MOCK_METHOD(content::PathInfo,
              GetPathInfo,
              (const base::FilePath& entry_path),
              (const, override));
  MOCK_METHOD(bool,
              IsValidLaunchParams,
              (const LaunchParams& params),
              (const, override));
};

class FakeWebLaunchService : public blink::mojom::WebLaunchService {
 public:
  FakeWebLaunchService() = default;
  ~FakeWebLaunchService() override = default;

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<blink::mojom::WebLaunchService>(
            std::move(handle)));
  }

  // blink::mojom::WebLaunchService:
  void EnqueueLaunchParams(
      const GURL& launch_url,
      base::TimeTicks time_navigation_started_in_browser,
      bool navigation_started,
      std::vector<blink::mojom::FileSystemAccessEntryPtr> files) override {
    launched_url_ = launch_url;
    enqueue_called_ = true;
    files_ = std::move(files);
  }

  bool enqueue_called() const { return enqueue_called_; }
  const GURL& launched_url() const { return launched_url_; }
  const std::vector<blink::mojom::FileSystemAccessEntryPtr>& files() const {
    return files_;
  }

  void Reset() {
    enqueue_called_ = false;
    launched_url_ = GURL();
    files_.clear();
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::WebLaunchService> receiver_{this};
  bool enqueue_called_ = false;
  GURL launched_url_;
  std::vector<blink::mojom::FileSystemAccessEntryPtr> files_;
};

class LaunchQueueTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto delegate =
        std::make_unique<testing::NiceMock<MockLaunchQueueDelegate>>();
    delegate_ = delegate.get();

    ON_CALL(*delegate_, IsValidLaunchParams)
        .WillByDefault(testing::Return(true));
    ON_CALL(*delegate_, IsInScope)
        .WillByDefault(
            [](const LaunchParams& params, const GURL& url) { return true; });

    launch_queue_ =
        std::make_unique<LaunchQueue>(web_contents(), std::move(delegate));

    InitTestApi(web_contents()->GetPrimaryMainFrame());
  }

  void TearDown() override {
    delegate_ = nullptr;
    launch_queue_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void InitTestApi(content::RenderFrameHost* rfh) {
    rfh->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        blink::mojom::WebLaunchService::Name_,
        base::BindRepeating(&FakeWebLaunchService::Bind,
                            base::Unretained(&fake_launch_service_)));
  }

 protected:
  LaunchParams CreateLaunchParams(const GURL& target_url,
                                  bool started_new_navigation = true) {
    LaunchParams params;
    params.set_target_url(target_url);
    params.set_started_new_navigation(started_new_navigation);
    params.set_app_id("test_app_id");
    return params;
  }

  // Commits a navigation to `url` and re-binds the fake WebLaunchService, since
  // committing may swap in a new RenderFrameHost. When `sandboxed` is true the
  // response carries a `Content-Security-Policy: sandbox` header, so the
  // committed document has an opaque origin while its URL stays unchanged.
  void CommitNavigation(const GURL& url, bool sandboxed) {
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateBrowserInitiated(url,
                                                             web_contents());
    if (sandboxed) {
      simulator->SetResponseHeaders(
          net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK")
              .AddHeader("Content-Security-Policy", "sandbox allow-scripts")
              .Build());
    }
    simulator->Commit();
    InitTestApi(web_contents()->GetPrimaryMainFrame());
  }

  std::unique_ptr<LaunchQueue> launch_queue_;
  raw_ptr<MockLaunchQueueDelegate> delegate_;
  FakeWebLaunchService fake_launch_service_;
};

TEST_F(LaunchQueueTest, EnqueueImmediatelyDispatches) {
  GURL launch_url("https://example.com/launch");
  CommitNavigation(launch_url, /*sandboxed=*/false);
  LaunchParams params =
      CreateLaunchParams(launch_url, /*started_new_navigation=*/false);

  launch_queue_->Enqueue(std::move(params));
  launch_queue_->FlushForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), launch_url);
}

TEST_F(LaunchQueueTest, EnqueueInvalidParams) {
  GURL launch_url("https://example.com/launch");
  CommitNavigation(launch_url, /*sandboxed=*/false);
  LaunchParams params = CreateLaunchParams(launch_url);
  params.set_paths({base::FilePath(FILE_PATH_LITERAL("sensitive_file.txt"))});

  EXPECT_CALL(*delegate_, IsValidLaunchParams(testing::_))
      .WillOnce(testing::Return(false));

  launch_queue_->Enqueue(std::move(params));
  launch_queue_->FlushForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_TRUE(fake_launch_service_.files().empty());
}

TEST_F(LaunchQueueTest, LaunchParamsDefaultWritePermissions) {
  LaunchParams params;
  params.set_paths({base::FilePath(FILE_PATH_LITERAL("file1.txt")),
                    base::FilePath(FILE_PATH_LITERAL("file2.txt"))});
  EXPECT_EQ(2u, params.paths().size());
  ASSERT_EQ(2u, params.can_write().size());
  EXPECT_FALSE(params.can_write()[0]);
  EXPECT_FALSE(params.can_write()[1]);

  params.set_paths_with_permissions(
      {base::FilePath(FILE_PATH_LITERAL("file3.txt")),
       base::FilePath(FILE_PATH_LITERAL("file4.txt"))},
      {true, false});
  EXPECT_EQ(2u, params.paths().size());
  ASSERT_EQ(2u, params.can_write().size());
  EXPECT_TRUE(params.can_write()[0]);
  EXPECT_FALSE(params.can_write()[1]);

  params.clear_paths();
  EXPECT_TRUE(params.paths().empty());
  EXPECT_TRUE(params.can_write().empty());
}

TEST_F(LaunchQueueTest, EnqueueValidParamsWithFiles) {
  GURL launch_url("https://example.com/launch");
  CommitNavigation(launch_url, /*sandboxed=*/false);
  ASSERT_FALSE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());

  LaunchParams params =
      CreateLaunchParams(launch_url, /*started_new_navigation=*/false);
  base::FilePath file_path(FILE_PATH_LITERAL("test_file.txt"));
  params.set_paths({file_path});

  EXPECT_CALL(*delegate_, GetPathInfo(file_path))
      .WillOnce(testing::Return(content::PathInfo(file_path)));

  launch_queue_->Enqueue(std::move(params));
  launch_queue_->FlushForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), launch_url);
  EXPECT_EQ(1u, fake_launch_service_.files().size());
}

TEST_F(LaunchQueueTest, EnqueueValidParamsWithDirectory) {
  GURL launch_url("https://example.com/launch");
  CommitNavigation(launch_url, /*sandboxed=*/false);

  LaunchParams params =
      CreateLaunchParams(launch_url, /*started_new_navigation=*/false);
  base::FilePath dir_path(FILE_PATH_LITERAL("test_dir"));
  params.set_dir(dir_path);

  EXPECT_CALL(*delegate_, GetPathInfo(dir_path))
      .WillOnce(testing::Return(content::PathInfo(dir_path)));

  launch_queue_->Enqueue(std::move(params));
  launch_queue_->FlushForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(1u, fake_launch_service_.files().size());
}

// Regression test for b/534356407: an in-scope document that committed an
// opaque origin (e.g. via `Content-Security-Policy: sandbox`) must not receive
// any launch params when reused in place: not file or directory handles minted
// for the app's origin, and not the target URL, which it was never navigated to
// and which may carry tokens.
TEST_F(LaunchQueueTest, OpaqueOriginReceivesNothingInPlace) {
  GURL launch_url("https://example.com/launch");
  // The document stays in scope by URL, but its origin is opaque.
  GURL sandboxed_url("https://example.com/preview");
  CommitNavigation(sandboxed_url, /*sandboxed=*/true);
  ASSERT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());

  LaunchParams params =
      CreateLaunchParams(launch_url, /*started_new_navigation=*/false);
  params.set_paths({base::FilePath(FILE_PATH_LITERAL("test_file.txt"))});
  params.set_dir(base::FilePath(FILE_PATH_LITERAL("test_dir")));

  // No entry should be minted, so no path should ever be resolved.
  EXPECT_CALL(*delegate_, GetPathInfo(testing::_)).Times(0);

  launch_queue_->Enqueue(std::move(params));
  launch_queue_->FlushForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

// A launch navigation can itself commit an opaque origin, e.g. if the launch
// URL is served with `Content-Security-Policy: sandbox`. This cannot be
// predicted before the response arrives, so the document must still receive
// nothing.
TEST_F(LaunchQueueTest, OpaqueOriginReceivesNothingAfterNavigation) {
  GURL launch_url("https://example.com/launch");
  CommitNavigation(launch_url, /*sandboxed=*/true);
  ASSERT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());

  LaunchParams params =
      CreateLaunchParams(launch_url, /*started_new_navigation=*/true);
  params.set_paths({base::FilePath(FILE_PATH_LITERAL("test_file.txt"))});
  params.set_dir(base::FilePath(FILE_PATH_LITERAL("test_dir")));

  EXPECT_CALL(*delegate_, GetPathInfo(testing::_)).Times(0);

  launch_queue_->Enqueue(std::move(params));
  launch_queue_->FlushForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

}  // namespace webapps
