// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_helper_delegate_installer.h"

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state_user_data.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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

  using SetDelegateCallback = base::RepeatingCallback<void(Delegate*)>;
  using SetSecondDelegateCallback =
      base::RepeatingCallback<void(SecondDelegate*)>;

  // Set callbacks.
  void SetCallbacks(SetDelegateCallback set_delegate_cb,
                    SetSecondDelegateCallback set_second_delegate_cb) {
    set_delegate_cb_ = set_delegate_cb;
    set_second_delegate_cb_ = set_second_delegate_cb;
  }

  // Accessors for the Delegate.
  void SetDelegate(Delegate* delegate) {
    delegate_ = delegate;
    if (!set_delegate_cb_.is_null())
      set_delegate_cb_.Run(delegate);
  }
  Delegate* GetDelegate() const { return delegate_; }

  // Accessors for the SecondDelegate.
  void SetSecondDelegate(SecondDelegate* delegate) {
    second_delegate_ = delegate;
    if (!set_second_delegate_cb_.is_null())
      set_second_delegate_cb_.Run(second_delegate_.get());
  }
  SecondDelegate* GetSecondDelegate() const { return second_delegate_; }

 private:
  explicit FakeTabHelper(web::WebState* web_state) {}
  friend class web::WebStateUserData<FakeTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // The delegates.
  raw_ptr<Delegate> delegate_ = nullptr;
  raw_ptr<SecondDelegate> second_delegate_ = nullptr;

  // The callbacks.
  SetDelegateCallback set_delegate_cb_;
  SetSecondDelegateCallback set_second_delegate_cb_;
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
  void WillActivateWebState(web::WebState* web_state) override {}
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
      : profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(
            profile_.get(),
            std::make_unique<FakeTabHelperWebStateListDelegate>())) {}
  ~TabHelperDelegateInstallerTest() override {}

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  Delegate delegate_;
  SecondDelegate second_delegate_;
};

// Tests that delegates are installed for WebStates that were added to the
// Browser before the installer was created.
TEST_F(TabHelperDelegateInstallerTest,
       InstallDelegatesForPreExistingTabHelpers) {
  // Insert a WebState into the WebStateList before the installer is created.
  browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());
  FakeTabHelper* tab_helper = FakeTabHelper::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));
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
  browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());
  FakeTabHelper* tab_helper = FakeTabHelper::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));

  // Verify that delegates were installed.
  EXPECT_EQ(tab_helper->GetDelegate(), &delegate_);
  EXPECT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);
}

// Tests that delegates are installed for WebState replacements.
TEST_F(TabHelperDelegateInstallerTest, InstallDelegatesForReplacedWebStates) {
  // Insert a WebState into the WebStateList before the installer is created.
  browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());
  FakeTabHelper* tab_helper = FakeTabHelper::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));

  // Create installers.
  DelegateInstaller installer(&delegate_, browser_.get());
  SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());
  ASSERT_EQ(tab_helper->GetDelegate(), &delegate_);
  ASSERT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);

  // Replace the WebState with a new one.
  std::unique_ptr<web::WebState> replaced_web_state =
      browser_->GetWebStateList()->ReplaceWebStateAt(
          0, std::make_unique<web::FakeWebState>());
  FakeTabHelper* replacement_tab_helper = FakeTabHelper::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));

  // Verify that the delegates were uninstalled from `tab_helper` and installed
  // for `replacement_tab_helper`.
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
  browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());
  FakeTabHelper* tab_helper = FakeTabHelper::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));

  // Create installers.
  DelegateInstaller installer(&delegate_, browser_.get());
  SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());
  ASSERT_EQ(tab_helper->GetDelegate(), &delegate_);
  ASSERT_EQ(tab_helper->GetSecondDelegate(), &second_delegate_);

  // Detach the WebState.
  std::unique_ptr<web::WebState> detached_web_state =
      browser_->GetWebStateList()->DetachWebStateAt(0);

  // Verify that the delegates were uninstalled from `tab_helper`.
  EXPECT_FALSE(tab_helper->GetDelegate());
  EXPECT_FALSE(tab_helper->GetSecondDelegate());
}

// Tests that delegates are uninstalled when the Browser is destroyed.
TEST_F(TabHelperDelegateInstallerTest,
       UninstallDelegatesForBrowserDestruction) {
  // Insert a WebState into the WebStateList before the installer is created.
  browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());

  Delegate* set_delegate = nullptr;
  SecondDelegate* set_second_delegate = nullptr;

  // Block to prevent using FakeTabHelper after the destruction of the Browser
  // instance (as the the object will have been destroyed by that point).
  {
    FakeTabHelper* tab_helper = FakeTabHelper::FromWebState(
        browser_->GetWebStateList()->GetWebStateAt(0));

    tab_helper->SetCallbacks(
        base::BindRepeating(
            [](Delegate** dest, Delegate* value) { *dest = value; },
            base::Unretained(&set_delegate)),
        base::BindRepeating(
            [](SecondDelegate** dest, SecondDelegate* value) { *dest = value; },
            base::Unretained(&set_second_delegate)));

    // Create installers.
    DelegateInstaller installer(&delegate_, browser_.get());
    SecondDelegateInstaller second_installer(&second_delegate_, browser_.get());
    ASSERT_EQ(set_delegate, &delegate_);
    ASSERT_EQ(set_second_delegate, &second_delegate_);

    // Destroy the Browser. This destroy the FakeTabHelper (as the WebState
    // it is attached to is stored in the Browser's WebStateList).
    browser_.reset();
  }

  // Verify that the delegates were uninstalled from `tab_helper`.
  EXPECT_FALSE(set_delegate);
  EXPECT_FALSE(set_second_delegate);
}

// Tests that delegates are uninstalled when the installers are destroyed.
TEST_F(TabHelperDelegateInstallerTest,
       UninstallDelegatesForInstallerDestruction) {
  // Insert a WebState into the WebStateList before the installer is created.
  browser_->GetWebStateList()->InsertWebState(
      std::make_unique<web::FakeWebState>());
  FakeTabHelper* tab_helper = FakeTabHelper::FromWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));

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

  // Verify that the delegates were uninstalled from `tab_helper`.
  EXPECT_FALSE(tab_helper->GetDelegate());
  EXPECT_FALSE(tab_helper->GetSecondDelegate());
}
