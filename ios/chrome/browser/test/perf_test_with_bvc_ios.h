// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEST_PERF_TEST_WITH_BVC_IOS_H_
#define IOS_CHROME_BROWSER_TEST_PERF_TEST_WITH_BVC_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/test/base/perf_test_ios.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/scoped_testing_web_client.h"

class Browser;
@class BrowserViewController;
@class BrowserViewControllerDependencyFactory;
@class CommandDispatcher;
@class TabModel;

// Base class for performance tests that require a browser view controller.  The
// BVC requires a non-trivial amount of setup and teardown, so it's best to
// derive from this class for tests that require a real BVC.  The class uses
// mock browser states and testing factories for AutocompleteClassifier.
class PerfTestWithBVC : public PerfTest {
 public:
  explicit PerfTestWithBVC(std::string testGroup);

  PerfTestWithBVC(std::string testGroup,
                  std::string firstLabel,
                  std::string averageLabel,
                  bool isWaterfall,
                  bool verbose,
                  bool slowTeardown,
                  int repeat);
  ~PerfTestWithBVC() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // True if the test needs extra time for Teardown.
  bool slow_teardown_;

  web::ScopedTestingWebClient web_client_;
  IOSChromeScopedTestingChromeBrowserProvider provider_;
  IOSChromeScopedTestingChromeBrowserStateManager browser_state_manager_;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestChromeBrowserState> incognito_chrome_browser_state_;

  BrowserWebStateListDelegate web_state_list_delegate_;

  WebStateList web_state_list_;
  WebStateList otr_web_state_list_;

  TabModel* tab_model_;
  TabModel* otr_tab_model_;

  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> otr_browser_;

  CommandDispatcher* command_dispatcher_;
  BrowserViewControllerDependencyFactory* bvc_factory_;
  BrowserViewController* bvc_;
  UIWindow* window_;
};

#endif  // IOS_CHROME_BROWSER_TEST_PERF_TEST_WITH_BVC_IOS_H_
