// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_

namespace content {

// Flags passed to the WebContentsDelegate.NavigationStateChanged to tell it
// what has changed. Combine them to update more than one thing.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.content_public.browser)
// GENERATED_JAVA_PREFIX_TO_STRIP: INVALIDATE_TYPE_
enum InvalidateTypes {
  INVALIDATE_TYPE_URL = 1 << 0,    // The URL has changed.
  INVALIDATE_TYPE_TAB = 1 << 1,    // The favicon, app icon, or crashed
                                   // state changed.
  INVALIDATE_TYPE_LOAD = 1 << 2,   // The loading state has changed.
  INVALIDATE_TYPE_TITLE = 1 << 3,  // The title changed.
  INVALIDATE_TYPE_AUDIO = 1 << 4,  // The tab became audible or
                                   // inaudible.
                                   // TODO(crbug.com/846374):
                                   // remove this.
  // Used only by NavigationStateChanged calls for committing a NavigationEntry.
  // Signifies that the initial NavigationEntry status will not be retained on
  // the committing NavigationEntry. This means the committing NavigationEntry
  // is not an initial NavigationEntry. See the comment below for more details.
  INVALIDATE_TYPE_REMOVE_INITIAL_NAVIGATION_ENTRY_STATUS = (1 << 5),
  // These INVALIDATE_TYPE_ALL values are fired when discarding pending entries
  // or committing a new entry, but which one is used depends on whether it's
  // for an initial NavigationEntry commit or not. If the NavigationEntry
  // about to be committed will be an initial NavigationEntry, then it will
  // retain the "initial NavigationEntry status" and fire the call with
  // INVALIDATE_TYPE_ALL_BUT_KEEPS_INITIAL_NAVIGATION_ENTRY_STATUS. Otherwise,
  // if the NavigationEntry will not be an initial NavigationEntry, the value
  // INVALIDATE_TYPE_ALL_AND_REMOVES_INITIAL_NAVIGATION_ENTRY_STATUS will be
  // used, which has the INVALIDATE_TYPE_REMOVE_INITIAL_NAVIGATION_ENTRY_STATUS
  // bit. This is needed for WebView, which should ignore the
  // INVALIDATE_TYPE_ALL_BUT_KEEPS_INITIAL_NAVIGATION_ENTRY_STATUS
  // NavigationStateChanged() calls to avoid firing onPageFinished etc in more
  // cases than it previously did. See also https://crbug.com/1277414.
  INVALIDATE_TYPE_ALL_BUT_KEEPS_INITIAL_NAVIGATION_ENTRY_STATUS = (1 << 5) - 1,
  INVALIDATE_TYPE_ALL_AND_REMOVES_INITIAL_NAVIGATION_ENTRY_STATUS =
      (1 << 6) - 1,
};
}

#endif  // CONTENT_PUBLIC_BROWSER_INVALIDATE_TYPE_H_
