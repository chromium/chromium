// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/content/navigable_contents_delegate.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/content/public/mojom/navigable_contents.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "services/content/service_delegate.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

class TestNavigableContentsClient : public mojom::NavigableContentsClient {
 public:
  TestNavigableContentsClient() = default;
  ~TestNavigableContentsClient() override = default;

 private:
  // mojom::NavigableContentsClient:
  void ClearViewFocus() override {}
  void DidFinishNavigation(const GURL& url,
                           bool is_main_frame,
                           bool is_error_page,
                           const scoped_refptr<net::HttpResponseHeaders>&
                               response_headers) override {}
  void DidStopLoading() override {}
  void DidAutoResizeView(const gfx::Size& new_size) override {}
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override {}
  void UpdateCanGoBack(bool can_go_back) override {}
  void UpdateContentAXTree(const ui::AXTreeID& id) override {}
  void FocusedNodeChanged(bool is_editable_node,
                          const gfx::Rect& node_bounds_in_screen) override {}

  DISALLOW_COPY_AND_ASSIGN(TestNavigableContentsClient);
};

class TestNavigableContentsDelegate : public NavigableContentsDelegate {
 public:
  TestNavigableContentsDelegate() = default;
  ~TestNavigableContentsDelegate() override = default;

  const GURL& last_navigated_url() const { return last_navigated_url_; }

  void set_navigation_callback(base::RepeatingClosure callback) {
    navigation_callback_ = std::move(callback);
  }

  // NavigableContentsDelegate:
  void Navigate(const GURL& url, mojom::NavigateParamsPtr params) override {
    last_navigated_url_ = url;
    if (navigation_callback_)
      navigation_callback_.Run();
  }

  void GoBack(
      content::mojom::NavigableContents::GoBackCallback callback) override {
    std::move(callback).Run(false /* success */);
  }

  void Focus() override {}
  void FocusThroughTabTraversal(bool reverse) override {}

  gfx::NativeView GetNativeView() override { return nullptr; }

 private:
  GURL last_navigated_url_;
  base::RepeatingClosure navigation_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigableContentsDelegate);
};

class TestServiceDelegate : public ServiceDelegate {
 public:
  TestServiceDelegate() = default;
  ~TestServiceDelegate() override = default;

  void set_navigable_contents_delegate_created_callback(
      base::RepeatingCallback<void(TestNavigableContentsDelegate*)> callback) {
    navigable_contents_delegate_created_callback_ = std::move(callback);
  }

  // ServiceDelegate:
  void WillDestroyServiceInstance(Service* service) override {}

  std::unique_ptr<NavigableContentsDelegate> CreateNavigableContentsDelegate(
      const mojom::NavigableContentsParams& params,
      mojom::NavigableContentsClient* client) override {
    auto delegate = std::make_unique<TestNavigableContentsDelegate>();
    if (navigable_contents_delegate_created_callback_)
      navigable_contents_delegate_created_callback_.Run(delegate.get());
    return delegate;
  }

 private:
  base::RepeatingCallback<void(TestNavigableContentsDelegate*)>
      navigable_contents_delegate_created_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestServiceDelegate);
};

class ContentServiceTest : public testing::Test {
 public:
  ContentServiceTest()
      : service_(&delegate_,
                 connector_factory_.RegisterInstance(mojom::kServiceName)) {}
  ~ContentServiceTest() override = default;

 protected:
  TestServiceDelegate& delegate() { return delegate_; }

  template <typename T>
  void ConnectReceiver(mojo::PendingReceiver<T> receiver) {
    connector_factory_.GetDefaultConnector()->Connect(
        content::mojom::kServiceName, std::move(receiver));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  service_manager::TestConnectorFactory connector_factory_;
  TestServiceDelegate delegate_;
  Service service_;

  DISALLOW_COPY_AND_ASSIGN(ContentServiceTest);
};

TEST_F(ContentServiceTest, NavigableContentsCreation) {
  mojo::Remote<mojom::NavigableContentsFactory> factory;
  ConnectReceiver(factory.BindNewPipeAndPassReceiver());

  base::RunLoop loop;

  TestNavigableContentsDelegate* navigable_contents_delegate = nullptr;
  delegate().set_navigable_contents_delegate_created_callback(
      base::BindLambdaForTesting([&](TestNavigableContentsDelegate* delegate) {
        EXPECT_FALSE(navigable_contents_delegate);
        navigable_contents_delegate = delegate;
        loop.Quit();
      }));

  mojo::Remote<mojom::NavigableContents> contents;
  TestNavigableContentsClient client_impl;
  mojo::Receiver<mojom::NavigableContentsClient> client_receiver(&client_impl);
  factory->CreateContents(mojom::NavigableContentsParams::New(),
                          contents.BindNewPipeAndPassReceiver(),
                          client_receiver.BindNewPipeAndPassRemote());
  loop.Run();

  base::RunLoop navigation_loop;
  ASSERT_TRUE(navigable_contents_delegate);
  navigable_contents_delegate->set_navigation_callback(
      navigation_loop.QuitClosure());

  const GURL kTestUrl("https://example.com/");
  contents->Navigate(kTestUrl, mojom::NavigateParams::New());
  navigation_loop.Run();

  EXPECT_EQ(kTestUrl, navigable_contents_delegate->last_navigated_url());
}

}  // namespace
}  // namespace content
