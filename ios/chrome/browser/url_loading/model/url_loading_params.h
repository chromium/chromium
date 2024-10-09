// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_PARAMS_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_PARAMS_H_

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ui/base/window_open_disposition.h"

// Enum of ways of changing loading behavior, that can be passed around
// opaquely, and set by using `UrlLoadParams::LoadStrategy`.
enum class UrlLoadStrategy {
  NORMAL = 0,

  ALWAYS_NEW_FOREGROUND_TAB = 1 << 0,
};

// UrlLoadingService wrapper around web::NavigationManager::WebLoadParams,
// WindowOpenDisposition and parameters from OpenNewTabCommand.
// This is used when a URL is opened.
struct UrlLoadParams {
 public:
  // Initializes a UrlLoadParams intended to open in current page.
  static UrlLoadParams InCurrentTab(
      const web::NavigationManager::WebLoadParams& web_params);
  static UrlLoadParams InCurrentTab(const GURL& url);
  static UrlLoadParams InCurrentTab(const GURL& url, const GURL& virtual_url);

  // Initializes a UrlLoadParams intended to open in a new page.
  static UrlLoadParams InNewTab(
      const web::NavigationManager::WebLoadParams& web_params);
  static UrlLoadParams InNewTab(const GURL& url);
  static UrlLoadParams InNewTab(const GURL& url, const GURL& virtual_url);
  static UrlLoadParams InNewTab(const GURL& url, int insertion_index);

  // Initializes a UrlLoadParams intended to switch to tab.
  static UrlLoadParams SwitchToTab(
      const web::NavigationManager::WebLoadParams& web_params);

  // Set appropriate parameters for background tab mode.
  void SetInBackground(bool in_background);

  // Allow copying UrlLoadParams.
  UrlLoadParams(const UrlLoadParams& other);
  UrlLoadParams& operator=(const UrlLoadParams& other);

  // The wrapped params.
  web::NavigationManager::WebLoadParams web_params =
      web::NavigationManager::WebLoadParams(GURL());

  // The disposition of the URL being opened. Defaults to
  // `WindowOpenDisposition::NEW_FOREGROUND_TAB`.
  WindowOpenDisposition disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  // Whether this requests opening in incognito or not. Defaults to `false`.
  bool in_incognito = false;

  // Location where the new tab should be opened. Defaults to
  // `OpenPosition::kLastTab`.
  OpenPosition append_to = OpenPosition::kLastTab;

  // Specific index where tab should be opened if `append_to` is
  // `OpenPosition::kSpecifiedIndex`
  int insertion_index = -1;

  // Origin point of the action triggering this command, in main window
  // coordinates. Defaults to `CGPointZero`.
  CGPoint origin_point = CGPointZero;

  // Whether or not this URL command comes from a chrome context (e.g.,
  // settings), as opposed to a web page context. Defaults to `false`.
  bool from_chrome = false;

  // Whether or not this URL command comes from an external source (e.g. a
  // widget).
  bool from_external = false;

  // Whether the new tab command was initiated by the user (e.g. by tapping the
  // new tab button in the tools menu) or not (e.g. opening a new tab via a
  // Javascript action). Defaults to `true`. Only used when the `web_params.url`
  // isn't valid.
  bool user_initiated = true;

  // Whether the new tab command should also trigger the omnibox to be focused.
  // Only used when the `web_params.url` isn't valid. Defaults to `false`.
  bool should_focus_omnibox = false;

  // Whether the new tab should inherit opener.
  bool inherit_opener = false;

  // Whether the URL should be loaded instantly. Defaults to `true`. If `false`,
  // it should NOT be loaded until activated.
  bool instant_load = true;

  // Opaque way of changing loading behavior.
  UrlLoadStrategy load_strategy = UrlLoadStrategy::NORMAL;

  // Tentative title of the new tab before the tab URL is loaded.
  // Note: Currently only applies to tabs that are not instant-loaded.
  std::u16string placeholder_title;

  // Whether the URL should be loaded as part of pinned tabs.
  bool load_pinned = false;

  // Whether the URL should be loaded as part of a tab group.
  bool load_in_group = false;

  // The tab group where the URL should be loaded (if null and `load_in_group`
  // the URL is loaded in a new tab group).
  base::WeakPtr<const TabGroup> tab_group;

  bool in_background() const {
    return disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  }

  // Public for testing only.
  UrlLoadParams();
  ~UrlLoadParams();
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_PARAMS_H_
