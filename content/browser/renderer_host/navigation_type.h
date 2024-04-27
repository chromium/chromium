// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TYPE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TYPE_H_

namespace content {

// Indicates different types of navigations that can occur that we will handle
// separately.
enum NavigationType {
  // Unknown type.
  NAVIGATION_TYPE_UNKNOWN,

  // A new entry was navigated to in the main frame. This covers all cases where
  // the main frame navigated and a new navigation entry was created. This means
  // cases like navigations to a hash on the same document or history.pushState
  // are MAIN_FRAME_NEW_ENTRY, except when it results in replacement (see
  // comment for NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY below).
  // This type of navigation will create a new NavigationEntry, without sharing
  // any (frame-specific) session history entries with other NavigationEntries.
  // Navigation entries created by subframe navigations are NEW_SUBFRAME.
  // Note: This includes all main frames (e.g. fenced frames), not only the
  // navigation entries created by navigations in primary main frames.
  // Navigation entries with this type will have a
  // `ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME` when this is a fenced
  // frame navigation.
  NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,

  // Navigating the main frame to an existing navigation entry. This is the case
  // for:
  // - Session history navigations
  // - Reloads, including reloads as a result of the user requesting a
  //   navigation to the same URL (e.g., pressing Enter in the URL bar)
  // - Same-document navigations that result in the current entry's replacement,
  //   as a result of history.replaceState(), location.replace(), and all
  //   same-document navigations on a document with a trivial session history
  //   entry requirement (e.g. prerender). Note that for normal non-replacement
  //   cases, same-document navigations on the main frame will be
  //   classified as NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY.
  //   TODO(crbug.com/40188865): Classify same-document replacements (or
  //   at least location.replace()) as NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY,
  //   like cross-document replacements and normal same-document navigations.
  //
  // This type of navigation will reuse the existing NavigationEntry but modify
  // most/all of the contents of the existing NavigationEntry. This means the
  // session history entry for the frame, which might be shared with other
  // NavigationEntries, will be reused in the updated NavigationEntry.
  // Note: This includes all main frames (e.g. fenced frames), not only the
  // navigation entries created by navigations in primary main frames.
  // TODO(crbug.com/40188865): Do not reuse the session history entry
  // for the frame (and maybe the NavigationEntry itself) for same-document
  // location.replace().
  // Navigation entries with this type will have a
  // `ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME` when this is a fenced
  // frame navigation.
  NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TYPE_H_
