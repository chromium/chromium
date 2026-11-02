// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_browser_agent.h"

#import <memory>
#import <utility>

#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class EnterpriseProxyBrowserAgentTest : public PlatformTest {
 protected:
  EnterpriseProxyBrowserAgentTest()
      : profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())) {}

  // Inserts a `FakeWebState` with an `EnterpriseProxyTabHelper` into
  // `browser_` and returns it.
  web::WebState* InsertWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    EnterpriseProxyTabHelper::CreateForWebState(web_state.get(),
                                                &error_service_);
    web::WebState* raw_web_state = web_state.get();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state), WebStateList::InsertionParams::Automatic());
    return raw_web_state;
  }

  // Detaches `web_state` from `browser_` and returns it.
  std::unique_ptr<web::WebState> DetachWebState(web::WebState* web_state) {
    WebStateList* web_state_list = browser_->GetWebStateList();
    return web_state_list->DetachWebStateAt(
        web_state_list->GetIndexOfWebState(web_state));
  }

  // Returns the delegate of the `EnterpriseProxyTabHelper` of `web_state`.
  EnterpriseProxyTabHelper::Delegate* GetDelegate(web::WebState* web_state) {
    return EnterpriseProxyTabHelper::FromWebState(web_state)->delegate();
  }

  // Returns the `EnterpriseProxyBrowserAgent` of `browser_`, if any.
  EnterpriseProxyBrowserAgent* browser_agent() {
    return EnterpriseProxyBrowserAgent::FromBrowser(browser_.get());
  }

  web::WebTaskEnvironment task_environment_;
  testing::NiceMock<enterprise_net::MockEnterpriseProxyService> proxy_service_;
  // Outlives the tab helpers, which are destroyed with `browser_`.
  enterprise_net::EnterpriseProxyErrorService error_service_{&proxy_service_};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

}  // namespace

// Tests that the agent becomes the delegate of the tabs inserted in its
// `Browser`.
TEST_F(EnterpriseProxyBrowserAgentTest, SetsDelegateOfInsertedWebStates) {
  EnterpriseProxyBrowserAgent::CreateForBrowser(browser_.get());

  web::WebState* web_state = InsertWebState();

  EXPECT_EQ(browser_agent(), GetDelegate(web_state));
}

// Tests that the delegate of a tab is reset once the tab is detached from the
// `Browser`.
TEST_F(EnterpriseProxyBrowserAgentTest, ResetsDelegateOfDetachedWebState) {
  EnterpriseProxyBrowserAgent::CreateForBrowser(browser_.get());
  web::WebState* web_state = InsertWebState();

  std::unique_ptr<web::WebState> detached_web_state = DetachWebState(web_state);

  EXPECT_EQ(nullptr, GetDelegate(detached_web_state.get()));
}

// Tests that the delegates of the tabs are reset once the agent is destroyed.
TEST_F(EnterpriseProxyBrowserAgentTest, ResetsDelegatesWhenDestroyed) {
  EnterpriseProxyBrowserAgent::CreateForBrowser(browser_.get());
  web::WebState* web_state = InsertWebState();

  EnterpriseProxyBrowserAgent::RemoveFromBrowser(browser_.get());

  EXPECT_EQ(nullptr, GetDelegate(web_state));
}

// Tests that the agent is the delegate of the WebStates outside its
// `WebStateList` whose dependencies `TabsDependencyInstallerManager` installs,
// e.g. Reader mode content, until they are uninstalled.
TEST_F(EnterpriseProxyBrowserAgentTest,
       SetsDelegateOfWebStatesInstalledByManager) {
  TabsDependencyInstallerManager::CreateForBrowser(browser_.get());
  EnterpriseProxyBrowserAgent::CreateForBrowser(browser_.get());
  TabsDependencyInstallerManager* manager =
      TabsDependencyInstallerManager::FromBrowser(browser_.get());
  web::FakeWebState web_state;
  EnterpriseProxyTabHelper::CreateForWebState(&web_state, &error_service_);

  manager->InstallDependencies(&web_state);
  EXPECT_EQ(browser_agent(), GetDelegate(&web_state));

  manager->UninstallDependencies(&web_state);
  EXPECT_EQ(nullptr, GetDelegate(&web_state));
}
