// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_TYPE_H_

namespace content {

// Indicates different types of navigations that can occur that we will handle
// separately.
enum NavigationType {
  // Unknown type.
  NAVIGATION_TYPE_UNKNOWN,

  // A new entry was navigated to in the main frame. This covers all cases where
  // the main frame navigated and a new navigation entry was created. This means
  // cases like navigations to a hash on the same document are NEW_ENTRY.
  // (Navigation entries created by subframe navigations are NEW_SUBFRAME.)
  NAVIGATION_TYPE_NEW_ENTRY,

  // Navigating to an existing navigation entry. This is the case for session
  // history navigations, reloads, and history.replaceState(). It also includes
  // reloads as a result of the user requesting a navigation to the same URL
  // (e.g., pressing Enter in the URL bar).
  NAVIGATION_TYPE_EXISTING_ENTRY,

  // A new subframe was manually navigated by the user. We will create a new
  // NavigationEntry so they can go back to the previous subframe content
  // using the back button.
  NAVIGATION_TYPE_NEW_SUBFRAME,

  // A subframe in the page was automatically loaded or navigated to such that
  // a new navigation entry should not be created. There are two cases:
  //  1. Stuff like iframes containing ads that the page loads automatically.
  //     The user doesn't want to see these, so we just update the existing
  //     navigation entry.
  //  2. Going back/forward to previous subframe navigations. We don't create
  //     a new entry here either, just update the last committed entry.
  // These two cases are actually pretty different, they just happen to
  // require almost the same code to handle.
  NAVIGATION_TYPE_AUTO_SUBFRAME,

  // Nothing happened. This happens when we get information about a page we
  // don't know anything about. It can also happen when an iframe in a popup
  // navigated to about:blank is navigated. Nothing needs to be done.
  NAVIGATION_TYPE_NAV_IGNORE,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_TYPE_H_
