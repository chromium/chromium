// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_INITIATION_TYPE_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_INITIATION_TYPE_H_

namespace web {

// Defines the ways how a pending navigation can be initiated.
enum class NavigationInitiationType {
  // Navigation initiation type is only valid for pending navigations, use NONE
  // if a navigation is already committed.
  NONE = 0,

  // Navigation was initiated by the browser by calling NavigationManager
  // methods. Examples of methods which cause browser-initiated navigations
  // include:
  //  * NavigationManager::Reload()
  //  * NavigationManager::GoBack()
  //  * NavigationManager::GoForward()
  BROWSER_INITIATED,

  // Navigation was initiated by renderer. Examples of renderer-initiated
  // navigations include:
  //  * <a> link click
  //  * changing window.location.href
  //  * redirect via the <meta http-equiv="refresh"> tag
  //  * using window.history.pushState
  RENDERER_INITIATED,
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_INITIATION_TYPE_H_
