// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

namespace web {

namespace {

// Class taking care for creating a window, connecting it to an arbitrary
// scene, making it key and visible, and restoring the UI to its previous
// state on destruction. The window has a root view controller and it can
// be used to add sub-views.
class ScopedWindowInstaller {
 public:
  // Non-copyable, non-moveable.
  ScopedWindowInstaller(const ScopedWindowInstaller&) = delete;
  ScopedWindowInstaller& operator=(const ScopedWindowInstaller&) = delete;

  ScopedWindowInstaller() {
    UIWindowScene* window_scene = nil;
    for (UIScene* scene in
         [[UIApplication sharedApplication] connectedScenes]) {
      window_scene = base::apple::ObjCCast<UIWindowScene>(scene);
      if (window_scene) {
        break;
      }
    }

    CHECK(window_scene) << "No UIWindowScene found!";
    original_key_window_ = window_scene.keyWindow;

    inserted_key_window_ = [[UIWindow alloc] initWithWindowScene:window_scene];
    inserted_key_window_.rootViewController = [[UIViewController alloc] init];
    [inserted_key_window_ makeKeyAndVisible];
  }

  ~ScopedWindowInstaller() {
    @autoreleasepool {
      [original_key_window_ makeKeyWindow];
      inserted_key_window_ = nil;
      original_key_window_ = nil;
    }
  }

  UIView* root_view() { return inserted_key_window_.rootViewController.view; }

 private:
  UIWindow* original_key_window_;
  UIWindow* inserted_key_window_;
};

// A WebStateObserver that allows waiting on DidFinishNavigation(...).
class WaitingWebStateObserver : public WebStateObserver {
 public:
  WaitingWebStateObserver(web::WebState* web_state) : web_state_(web_state) {
    web_state_->AddObserver(this);
  }

  ~WaitingWebStateObserver() override {
    if (web_state_) {
      web_state_->RemoveObserver(this);
      web_state_ = nullptr;
    }
  }

  void SetOnDidFinishNavigation(base::OnceClosure closure) {
    CHECK(!closure_);
    CHECK(!closure.is_null());
    closure_ = std::move(closure);
  }

  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* context) override {
    CHECK_EQ(web_state_, web_state);
    if (closure_) {
      std::move(closure_).Run();
    }
  }

  void WebStateDestroyed(web::WebState* web_state) override {
    CHECK_EQ(web_state_, web_state);
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }

 private:
  raw_ptr<web::WebState> web_state_;
  base::OnceClosure closure_;
};

}  // namespace

class WebStateImplIntTest : public WebTestWithWebState {
 protected:
  WebStateImplIntTest() { RegisterDefaultHandlers(&server_); }

  void SetUp() override {
    WebTestWithWebState::SetUp();
    ASSERT_TRUE(server_.Start());
  }

  net::EmbeddedTestServer server_;
};

// Test validating that NavigationManagerImpl::Restore(...) does not crash
// if the WebState's Web usage is disabled. This is a reproduction case for
// https://crbug.com/532898037 validating that the fix is working.
TEST_F(WebStateImplIntTest, NoCrashWhenNavigatingWhileWebUsageIsDisabled) {
  ScopedWindowInstaller window_installer;

  // Ensure the WKWebView is inserted in the view hierarchy (in case this is
  // required for the navigation to proceed).
  UIView* web_view = web_state()->GetView();
  web_view.frame = window_installer.root_view().frame;
  [window_installer.root_view() addSubview:web_view];

  // To reproduce https://crbug.com/532898037, the WebState should be in the
  // following state: have at least one committed navigation, have a pending
  // navigation, have IsWebUsageEnabled() return false. In those conditions,
  // calling web_state->GetNavigationManager()->LoadURLWithParams(...) would
  // crash while trying to restore the navigation when web usage is disabled.
  //
  // So the test will reproduce those steps by doing a first navigation and
  // waiting for it to complete, then starting a second navigation and then
  // immediately disabling the web usage and attempting another navigation.
  //
  // To confirm that the test pass, it will then re-enable the web usage and
  // check that the second navigation completes.
  auto observer = std::make_unique<WaitingWebStateObserver>(web_state());

  {
    // First navigation, it must complete, so use the observer to wait for it.
    base::RunLoop run_loop;
    observer->SetOnDidFinishNavigation(run_loop.QuitClosure());

    web_state()->GetNavigationManager()->LoadURLWithParams(
        NavigationManager::WebLoadParams(server_.GetURL("/echo?q=0")));

    run_loop.Run();
  }

  // Second navigation, will be pending as we do not wait for it.
  web_state()->GetNavigationManager()->LoadURLWithParams(
      NavigationManager::WebLoadParams(server_.GetURL("/echo?q=1")));

  {
    // Third navigation, that will be attempted after disabling the web usage.
    // It must not crash during the call to LoadURLWithParams(
    base::RunLoop run_loop;
    observer->SetOnDidFinishNavigation(run_loop.QuitClosure());

    web_state()->SetWebUsageEnabled(false);
    web_state()->GetNavigationManager()->LoadURLWithParams(
        NavigationManager::WebLoadParams(server_.GetURL("/echo?q=2")));

    // Enable the web usage, and request loading the pending items if needed.
    // Then wait for the navigation to complete.
    web_state()->SetWebUsageEnabled(true);
    web_state()->GetNavigationManager()->LoadIfNecessary();

    run_loop.Run();
  }
}

}  // namespace web
