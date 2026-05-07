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
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"

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
  }

  bool enqueue_called() const { return enqueue_called_; }
  const GURL& launched_url() const { return launched_url_; }

  void Reset() {
    enqueue_called_ = false;
    launched_url_ = GURL();
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::WebLaunchService> receiver_{this};
  bool enqueue_called_ = false;
  GURL launched_url_;
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
    params.target_url = target_url;
    params.started_new_navigation = started_new_navigation;
    params.app_id = "test_app_id";
    return params;
  }

  std::unique_ptr<LaunchQueue> launch_queue_;
  raw_ptr<MockLaunchQueueDelegate> delegate_;
  FakeWebLaunchService fake_launch_service_;
};

TEST_F(LaunchQueueTest, EnqueueAndCommit) {
  GURL launch_url("https://example.com/launch");
  LaunchParams params = CreateLaunchParams(launch_url);

  launch_queue_->Enqueue(std::move(params));
  EXPECT_TRUE(launch_queue_->GetPendingLaunchAppId());

  // Simulate successful navigation commit using NavigationSimulator.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             launch_url);

  launch_queue_->FlushForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), launch_url);
  EXPECT_FALSE(launch_queue_->GetPendingLaunchAppId());
}

TEST_F(LaunchQueueTest, EnqueueAndAbort) {
  GURL launch_url("https://example.com/launch");
  LaunchParams params = CreateLaunchParams(launch_url);

  launch_queue_->Enqueue(std::move(params));
  EXPECT_TRUE(launch_queue_->GetPendingLaunchAppId());

  // Simulate aborted navigation (does not commit).
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), launch_url, net::ERR_ABORTED);

  launch_queue_->FlushForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
  EXPECT_FALSE(
      launch_queue_->GetPendingLaunchAppId());  // Queue should be reset
}

TEST_F(LaunchQueueTest, EnqueueAndError) {
  GURL launch_url("https://example.com/launch");
  LaunchParams params = CreateLaunchParams(launch_url);

  launch_queue_->Enqueue(std::move(params));
  EXPECT_TRUE(launch_queue_->GetPendingLaunchAppId());

  // Simulate navigation that commits an error page.
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), launch_url, net::ERR_CONNECTION_RESET);

  launch_queue_->FlushForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
  EXPECT_FALSE(
      launch_queue_->GetPendingLaunchAppId());  // Queue should be reset
}

TEST_F(LaunchQueueTest, EnqueueAndOutOfScope) {
  GURL launch_url("https://example.com/launch");
  GURL out_of_scope_url("https://attacker.com/");
  LaunchParams params = CreateLaunchParams(launch_url);

  launch_queue_->Enqueue(std::move(params));
  EXPECT_TRUE(launch_queue_->GetPendingLaunchAppId());

  // Delegate says it is out of scope.
  EXPECT_CALL(*delegate_, IsInScope(testing::_, out_of_scope_url))
      .WillOnce(testing::Return(false));

  // Simulate navigation to out of scope URL.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             out_of_scope_url);

  launch_queue_->FlushForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
  EXPECT_FALSE(
      launch_queue_->GetPendingLaunchAppId());  // Queue should be reset
}

}  // namespace webapps
