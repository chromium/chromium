// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_helper_delegate_installer.h"

#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A fake tab helper delegate.
class Delegate {
 public:
  Delegate() = default;
  ~Delegate() = default;
};
// A second fake tab helper delegate.
class SecondDelegate {
 public:
  SecondDelegate() = default;
  ~SecondDelegate() = default;
};
// Tab helper for use in tests.
class FakeTabHelper : public web::WebStateUserData<FakeTabHelper> {
 public:
  ~FakeTabHelper() override {}

  // Accessors for the Delegate.
  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* GetDelegate() const { return delegate_; }

  // Accessors for the SecondDelegate.
  void SetSecondDelegate(SecondDelegate* delegate) {
    second_delegate_ = delegate;
  }
  SecondDelegate* GetSecondDelegate() const { return second_delegate_; }

 private:
  explicit FakeTabHelper(web::WebState* web_state) {}
  friend class web::WebStateUserData<FakeTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // The delegates.
  Delegate* delegate_ = nullptr;
  SecondDelegate* second_delegate_ = nullptr;
};
WEB_STATE_USER_DATA_KEY_IMPL(FakeTabHelper)
// WebStateListDelegate that installs FakeTabHelpers.
class FakeTabHelperWebStateListDelegate : public WebStateListDelegate {
 public:
  FakeTabHelperWebStateListDelegate() {}
  ~FakeTabHelperWebStateListDelegate() override {}

  void WillAddWebState(web::WebState* web_state) override {
    FakeTabHelper::CreateForWebState(web_state);
  }
  void WebStateDetached(web::WebState* web_state) override {}
};
}  // namespace

// Installer types used in tests.  Defined as typedefs to improve formatting.
using DelegateInstaller = TabHelperDelegateInstaller<FakeTabHelper, Delegate>;
using SecondDelegateInstaller =
    TabHelperDelegateInstaller<FakeTabHelper,
                               SecondDelegate,
                               &FakeTabHelper::SetSecondDelegate>;

// Test fixture for TabHelperDelegateInstaller.
class TabHelperDelegateInstallerTest : public PlatformTest {
 protected:
  TabHelperDelegateInstallerTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        web_state_list_(&web_state_list_delegate_),
        browser_(std::make_unique<TestBrowser>(browser_state_.get(),
                                               &web_state_list_)) {}
  ~TabHelperDelegateInstallerTest() override {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  FakeTabHelperWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<Browser> browser_;
  Delegate delegate_;
  SecondDelegate second_delegate_;
};

// Tests that delegates are installed for WebStates that were added to the
// Browser before the installer was created.
TEST_F(TabHelperDelegateInstallerTest,
       InstallDelegatesForPreExistingTabHelpers) {
  // Insert a WebState into the WebStateList before the installer is created.
  web_state_list_.InsertWebState(0, std::make_unique<web::FakeWebState>(),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  FakeTabHelper* tab_helper =
      FakeTabHelper::FromWebState(web_state_list_.GetWebStateAt(0));
  ASSERT_FALSE(tab_helper->GetDelegate());
  ASSERT_FALSE(tab_helper->GetSecondDelegate());

  // Create installers.
  DelegateInstaller installer(&delegate_, browser_.get());
  SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());

  // Verify that delegates were installed.
  EXPECT_EQ(tab_helper->GetDelegate(), &delegate_);
  EXPECT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);
}

// Tests that delegates are installed for WebStates that were added to the
// Browser after the installer was created.
TEST_F(TabHelperDelegateInstallerTest, InstallDelegatesForAddedWebStates) {
  // Create installers.
  DelegateInstaller installer(&delegate_, browser_.get());
  SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());

  // Insert a WebState into the WebStateList.
  web_state_list_.InsertWebState(0, std::make_unique<web::FakeWebState>(),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  FakeTabHelper* tab_helper =
      FakeTabHelper::FromWebState(web_state_list_.GetWebStateAt(0));

  // Verify that delegates were installed.
  EXPECT_EQ(tab_helper->GetDelegate(), &delegate_);
  EXPECT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);
}

// Tests that delegates are installed for WebState replacements.
TEST_F(TabHelperDelegateInstallerTest, InstallDelegatesForReplacedWebStates) {
  // Insert a WebState into the WebStateList before the installer is created.
  web_state_list_.InsertWebState(0, std::make_unique<web::FakeWebState>(),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  FakeTabHelper* tab_helper =
      FakeTabHelper::FromWebState(web_state_list_.GetWebStateAt(0));

  // Create installers.
  DelegateInstaller installer(&delegate_, browser_.get());
  SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());
  ASSERT_EQ(tab_helper->GetDelegate(), &delegate_);
  ASSERT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);

  // Replace the WebState with a new one.
  std::unique_ptr<web::WebState> replaced_web_state =
      web_state_list_.ReplaceWebStateAt(0,
                                        std::make_unique<web::FakeWebState>());
  FakeTabHelper* replacement_tab_helper =
      FakeTabHelper::FromWebState(web_state_list_.GetWebStateAt(0));

  // Verify that the delegates were uninstalled from |tab_helper| and installed
  // for |replacement_tab_helper|.
  EXPECT_FALSE(tab_helper->GetDelegate());
  EXPECT_FALSE(tab_helper->GetSecondDelegate());
  EXPECT_EQ(replacement_tab_helper->GetDelegate(), &delegate_);
  EXPECT_EQ(replacement_tab_helper->GetSecondDelegate(), &second_delegate_);
}

// Tests that delegates are uninstalled from WebStates that were detached from
// the Browser's WebStateList.
TEST_F(TabHelperDelegateInstallerTest,
       UninstallDelegatesFromDetachedWebStates) {
  // Insert a WebState into the WebStateList before the installer is created.
  web_state_list_.InsertWebState(0, std::make_unique<web::FakeWebState>(),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  FakeTabHelper* tab_helper =
      FakeTabHelper::FromWebState(web_state_list_.GetWebStateAt(0));

  // Create installers.
  DelegateInstaller installer(&delegate_, browser_.get());
  SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());
  ASSERT_EQ(tab_helper->GetDelegate(), &delegate_);
  ASSERT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);

  // Detach the WebState.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list_.DetachWebStateAt(0);

  // Verify that the delegates were uninstalled from |tab_helper|.
  EXPECT_FALSE(tab_helper->GetDelegate());
  EXPECT_FALSE(tab_helper->GetSecondDelegate());
}

// Tests that delegates are uninstalled when the Browser is destroyed.
TEST_F(TabHelperDelegateInstallerTest,
       UninstallDelegatesForBrowserDestruction) {
  // Insert a WebState into the WebStateList before the installer is created.
  web_state_list_.InsertWebState(0, std::make_unique<web::FakeWebState>(),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  FakeTabHelper* tab_helper =
      FakeTabHelper::FromWebState(web_state_list_.GetWebStateAt(0));

  // Create installers.
  DelegateInstaller installer(&delegate_, browser_.get());
  SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());
  ASSERT_EQ(tab_helper->GetDelegate(), &delegate_);
  ASSERT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);

  // Destroy the Browser.
  browser_ = nullptr;

  // Verify that the delegates were uninstalled from |tab_helper|.
  EXPECT_FALSE(tab_helper->GetDelegate());
  EXPECT_FALSE(tab_helper->GetSecondDelegate());
}

// Tests that delegates are uninstalled when the installers are destroyed.
TEST_F(TabHelperDelegateInstallerTest,
       UninstallDelegatesForInstallerDestruction) {
  // Insert a WebState into the WebStateList before the installer is created.
  web_state_list_.InsertWebState(0, std::make_unique<web::FakeWebState>(),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  FakeTabHelper* tab_helper =
      FakeTabHelper::FromWebState(web_state_list_.GetWebStateAt(0));

  // Create installers.
  std::unique_ptr<DelegateInstaller> installer =
      std::make_unique<DelegateInstaller>(&delegate_, browser_.get());
  std::unique_ptr<SecondDelegateInstaller> second_installer =
      std::make_unique<SecondDelegateInstaller>(&second_delegate_,
                                                browser_.get());
  ASSERT_EQ(tab_helper->GetDelegate(), &delegate_);
  ASSERT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);

  // Destroy the installers.
  installer = nullptr;
  second_installer = nullptr;

  // Verify that the delegates were uninstalled from |tab_helper|.
  EXPECT_FALSE(tab_helper->GetDelegate());
  EXPECT_FALSE(tab_helper->GetSecondDelegate());
}
