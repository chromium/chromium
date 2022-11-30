// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_TEST_NAVIGATION_MANAGER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_TEST_NAVIGATION_MANAGER_H_

#import "ios/web/public/test/fakes/fake_navigation_manager.h"

class ToolbarTestNavigationManager : public web::FakeNavigationManager {
 public:
  ToolbarTestNavigationManager();

  bool CanGoBack() const override;
  bool CanGoForward() const override;

  void set_can_go_back(bool can_go_back);
  void set_can_go_forward(bool can_go_forward);

 private:
  bool can_go_back_;
  bool can_go_forward_;
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_TEST_NAVIGATION_MANAGER_H_
