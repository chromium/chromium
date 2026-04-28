// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation/referrer.h"
#import "url/gurl.h"

UrlLoadParams UrlLoadParams::InCurrentTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.web_params = web_params;
  return params;
}

UrlLoadParams UrlLoadParams::InCurrentTab(const GURL& url,
                                          const GURL& virtual_url) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.web_params = web::NavigationManager::WebLoadParams(url);
  params.web_params.virtual_url = virtual_url;
  return params;
}

UrlLoadParams UrlLoadParams::InCurrentTab(const GURL& url) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.web_params = web::NavigationManager::WebLoadParams(url);
  return params;
}

UrlLoadParams UrlLoadParams::InNewTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams params = UrlLoadParams();
  params.web_params = web_params;
  return params;
}

UrlLoadParams UrlLoadParams::InNewTab(const GURL& url,
                                      const GURL& virtual_url) {
  UrlLoadParams params = UrlLoadParams();
  params.web_params = web::NavigationManager::WebLoadParams(url);
  params.web_params.virtual_url = virtual_url;
  return params;
}

UrlLoadParams UrlLoadParams::InNewTab(const GURL& url) {
  UrlLoadParams params = UrlLoadParams();
  params.web_params = web::NavigationManager::WebLoadParams(url);
  return params;
}

UrlLoadParams UrlLoadParams::InNewTab(const GURL& url, int insertion_index) {
  UrlLoadParams params = UrlLoadParams();
  params.web_params = web::NavigationManager::WebLoadParams(url);
  params.append_to = OpenPosition::kSpecifiedIndex;
  params.insertion_index = insertion_index;
  return params;
}

UrlLoadParams UrlLoadParams::FromOpenNewTabCommand(OpenNewTabCommand* command) {
  UrlLoadParams params =
      UrlLoadParams::InNewTab(command.URL, command.virtualURL);
  if (command.textFragment.length) {
    params.web_params.internal_scroll_to_text_fragment =
        base::SysNSStringToUTF8(command.textFragment);
  }
  params.SetInBackground(command.inBackground);
  params.web_params.referrer = command.referrer;
  params.web_params.extra_headers = [command.extraHeaders copy];
  params.in_incognito = command.inIncognito;
  params.append_to = command.appendTo;
  params.origin_point = command.originPoint;
  params.from_chrome = command.fromChrome;
  params.send_tab_to_self_entry_guid =
      base::SysNSStringToUTF8(command.sendTabToSelfEntryGUID);
  params.user_initiated = command.userInitiated;
  params.should_focus_omnibox = command.shouldFocusOmnibox;
  params.inherit_opener = !command.inBackground;
  return params;
}

UrlLoadParams UrlLoadParams::SwitchToTab(
    const web::NavigationManager::WebLoadParams& web_params) {
  UrlLoadParams params = UrlLoadParams();
  params.disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  params.web_params = web_params;
  return params;
}

UrlLoadParams::UrlLoadParams() = default;

UrlLoadParams::~UrlLoadParams() = default;

UrlLoadParams::UrlLoadParams(const UrlLoadParams& other) = default;

UrlLoadParams& UrlLoadParams::operator=(const UrlLoadParams& other) = default;

void UrlLoadParams::SetInBackground(bool in_background) {
  this->disposition = in_background ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                                    : WindowOpenDisposition::NEW_FOREGROUND_TAB;
}
