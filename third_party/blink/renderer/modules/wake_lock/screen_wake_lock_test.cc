// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/screen_wake_lock.h"

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

#include <memory>

namespace blink {
namespace {

using device::mojom::blink::WakeLock;
using device::mojom::blink::WakeLockRequest;

// A mock WakeLock used to intercept calls to the mojo methods.
class MockWakeLock : public WakeLock {
 public:
  MockWakeLock() : binding_(this) {}
  ~MockWakeLock() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(WakeLockRequest(std::move(handle)));
  }

  bool WakeLockStatus() { return wake_lock_status_; }

 private:
  // mojom::WakeLock
  void RequestWakeLock() override { wake_lock_status_ = true; }
  void CancelWakeLock() override { wake_lock_status_ = false; }
  void AddClient(device::mojom::blink::WakeLockRequest wake_lock) override {}
  void ChangeType(device::mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {}
  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {}

  mojo::Binding<WakeLock> binding_;
  bool wake_lock_status_ = false;
};

class ScreenWakeLockTest : public testing::Test {
 protected:
  void SetUp() override {
    service_manager::InterfaceProvider::TestApi test_api(
        test_web_frame_client_.GetInterfaceProvider());
    test_api.SetBinderForName(
        WakeLock::Name_, WTF::BindRepeating(&MockWakeLock::Bind,
                                            WTF::Unretained(&mock_wake_lock_)));

    web_view_helper_.Initialize(&test_web_frame_client_);
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8("http://example.com/"), test::CoreTestDataPath(),
        WebString::FromUTF8("foo.html"));
    LoadFrame();
  }

  void TearDown() override {
    platform_->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
    test::RunPendingTasks();
  }

  void LoadFrame() {
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(),
        "http://example.com/foo.html");
    web_view_helper_.GetWebView()->UpdateAllLifecyclePhases();
  }

  LocalFrame* GetFrame() {
    DCHECK(web_view_helper_.GetWebView());
    DCHECK(web_view_helper_.GetWebView()->MainFrameImpl());
    return web_view_helper_.GetWebView()->MainFrameImpl()->GetFrame();
  }

  Screen* GetScreen() {
    DCHECK(GetFrame());
    DCHECK(GetFrame()->DomWindow());
    return GetFrame()->DomWindow()->screen();
  }

  bool ScreenKeepAwake() {
    DCHECK(GetScreen());
    return ScreenWakeLock::keepAwake(*GetScreen());
  }

  bool ClientKeepScreenAwake() { return mock_wake_lock_.WakeLockStatus(); }

  void SetKeepAwake(bool keepAwake) {
    DCHECK(GetScreen());
    ScreenWakeLock::setKeepAwake(*GetScreen(), keepAwake);
    // Let the notification sink through the mojo pipes.
    test::RunPendingTasks();
  }

  void Show() {
    DCHECK(web_view_helper_.GetWebView());
    web_view_helper_.GetWebView()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kVisible, false);
    // Let the notification sink through the mojo pipes.
    test::RunPendingTasks();
  }

  void Hide() {
    DCHECK(web_view_helper_.GetWebView());
    web_view_helper_.GetWebView()->SetVisibilityState(
        mojom::blink::PageVisibilityState::kHidden, false);
    // Let the notification sink through the mojo pipes.
    test::RunPendingTasks();
  }

  // Order of these members is important as we need to make sure that
  // test_web_frame_client_ outlives web_view_helper_ (destruction order)
  frame_test_helpers::TestWebFrameClient test_web_frame_client_;
  frame_test_helpers::WebViewHelper web_view_helper_;

  MockWakeLock mock_wake_lock_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_F(ScreenWakeLockTest, setAndReset) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  SetKeepAwake(true);
  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_TRUE(ClientKeepScreenAwake());

  SetKeepAwake(false);
  EXPECT_FALSE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, hideWhenSet) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  SetKeepAwake(true);
  Hide();

  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, setWhenHidden) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  Hide();
  SetKeepAwake(true);

  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, showWhenSet) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  Hide();
  SetKeepAwake(true);
  Show();

  EXPECT_TRUE(ScreenKeepAwake());
  EXPECT_TRUE(ClientKeepScreenAwake());
}

TEST_F(ScreenWakeLockTest, navigate) {
  ASSERT_FALSE(ScreenKeepAwake());
  ASSERT_FALSE(ClientKeepScreenAwake());

  SetKeepAwake(true);
  LoadFrame();

  EXPECT_FALSE(ScreenKeepAwake());
  EXPECT_FALSE(ClientKeepScreenAwake());
}

}  // namespace
}  // namespace blink
