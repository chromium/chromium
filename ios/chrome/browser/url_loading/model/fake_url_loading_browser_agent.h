// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_FAKE_URL_LOADING_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_FAKE_URL_LOADING_BROWSER_AGENT_H_

#include "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

class FakeUrlLoadingBrowserAgent : public UrlLoadingBrowserAgent {
 public:
  // Injects an instance attached to `browser`, using the superclass user data
  // key.
  static void InjectForBrowser(Browser* browser);

  static FakeUrlLoadingBrowserAgent* FromUrlLoadingBrowserAgent(
      UrlLoadingBrowserAgent*);

  // These are the last parameters passed to `SwitchToTab`,
  // `LoadUrlInCurrentTab` or `LoadUrlInNewTab`.
  UrlLoadParams last_params;

  // Call counts for overridden methods.
  int load_current_tab_call_count = 0;
  int switch_tab_call_count = 0;
  int load_new_tab_call_count = 0;

 private:
  explicit FakeUrlLoadingBrowserAgent(Browser* browser);

  // Switches to a tab that matches `params.web_params` or opens in a new tab.
  void SwitchToTab(const UrlLoadParams& params) override;

  // Opens a url based on `params` in current tab.
  void LoadUrlInCurrentTab(const UrlLoadParams& params) override;

  // Opens a url based on `params` in a new tab.
  void LoadUrlInNewTab(const UrlLoadParams& params) override;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_FAKE_URL_LOADING_BROWSER_AGENT_H_
