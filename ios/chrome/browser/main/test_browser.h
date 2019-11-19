// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
#define IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_

#include "ios/chrome/browser/main/browser.h"

#include "base/macros.h"
#include "base/observer_list.h"

class TestBrowser : public Browser {
 public:
  // Constructor that takes a TabModel.  TestBrowsers created using this
  // constructor will return return the |tab_model|'s WebStateList for
  // GetWebStateList().  DEPRECATED: Use this constructor only to test legacy
  // code that has not been updated to use WebStateList.
  TestBrowser(ios::ChromeBrowserState* browser_state, TabModel* tab_model);
  // Constructor that takes a WebStateList.  TestBrowsers created using this
  // constructor will return nil for GetTabModel().
  TestBrowser(ios::ChromeBrowserState* browser_state,
              WebStateList* web_state_list);
  ~TestBrowser() override;

  // Browser.
  ios::ChromeBrowserState* GetBrowserState() const override;
  TabModel* GetTabModel() const override;
  WebStateList* GetWebStateList() const override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
  ios::ChromeBrowserState* browser_state_ = nullptr;
  TabModel* tab_model_ = nil;
  WebStateList* web_state_list_ = nullptr;
  base::ObserverList<BrowserObserver, /* check_empty= */ true> observers_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowser);
};

#endif  // IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
