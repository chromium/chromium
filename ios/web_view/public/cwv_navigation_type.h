// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_TYPE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_TYPE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Types of transitions between pages.
typedef NS_OPTIONS(NSUInteger, CWVNavigationType) {
  // User got to this page by clicking a link on another page.
  CWVNavigationTypeLink = 0,

  // User got this page by typing the URL in the URL bar.  This should not be
  // used for cases where the user selected a choice that didn't look at all
  // like a URL; see GENERATED below.
  //
  // We also use this for other "explicit" navigation actions.
  CWVNavigationTypeTyped = 1,

  // User got to this page through a suggestion in the UI, for example)
  // through the destinations page.
  CWVNavigationTypeAutoBookmark = 2,

  // This is a subframe navigation. This is any content that is automatically
  // loaded in a non-toplevel frame. For example, if a page consists of
  // several frames containing ads, those ad URLs will have this transition
  // type. The user may not even realize the content in these pages is a
  // separate frame, so may not care about the URL (see MANUAL below).
  CWVNavigationTypeAutoSubframe = 3,

  // For subframe navigations that are explicitly requested by the user and
  // generate new navigation entries in the back/forward list. These are
  // probably more important than frames that were automatically loaded in
  // the background because the user probably cares about the fact that this
  // link was loaded.
  CWVNavigationTypeManualSubframe = 4,

  // User got to this page by typing in the URL bar and selecting an entry
  // that did not look like a URL.  For example, a match might have the URL
  // of a Google search result page, but appear like "Search Google for ...".
  // These are not quite the same as TYPED navigations because the user
  // didn't type or see the destination URL.
  // See also KEYWORD.
  CWVNavigationTypeGenerated = 5,

  // This is a toplevel navigation. This is any content that is automatically
  // loaded in a toplevel frame.  For example, opening a tab to show the ASH
  // screen saver, opening the devtools window, opening the NTP after the safe
  // browsing warning, opening web-based dialog boxes are examples of
  // AUTO_TOPLEVEL navigations.
  CWVNavigationTypeAutoToplevel = 6,

  // The user filled out values in a form and submitted it. NOTE that in
  // some situations submitting a form does not result in this transition
  // type. This can happen if the form uses script to submit the contents.
  CWVNavigationTypeFormSubmit = 7,

  // The user "reloaded" the page, either by hitting the reload button or by
  // hitting enter in the address bar.  NOTE: This is distinct from the
  // concept of whether a particular load uses "reload semantics" (i.e.
  // bypasses cached data).  For this reason, lots of code needs to pass
  // around the concept of whether a load should be treated as a "reload"
  // separately from their tracking of this transition type, which is mainly
  // used for proper scoring for consumers who care about how frequently a
  // user typed/visited a particular URL.
  //
  // SessionRestore and undo tab close use this transition type too.
  CWVNavigationTypeReload = 8,

  // The url was generated from a replaceable keyword other than the default
  // search provider. If the user types a keyword (which also applies to
  // tab-to-search) in the omnibox this qualifier is applied to the transition
  // type of the generated url. TemplateURLModel then may generate an
  // additional visit with a transition type of KEYWORD_GENERATED against the
  // url 'http://' + keyword. For example, if you do a tab-to-search against
  // wikipedia the generated url has a transition qualifer of KEYWORD, and
  // TemplateURLModel generates a visit for 'wikipedia.org' with a transition
  // type of KEYWORD_GENERATED.
  CWVNavigationTypeKeyword = 9,

  // Corresponds to a visit generated for a keyword. See description of
  // KEYWORD for more details.
  CWVNavigationTypeKeywordGenerated = 10,

  // Corresponds to a navigation causing the new web view to be created.
  // It's only used in [web_view.UIDelegate
  // webView:createWebViewWithConfiguration:forNavigationAction:]
  CWVNavigationTypeNewWindow = 11,

  // ADDING NEW CORE VALUE? Be sure to update ui::PageTransition too.
  CWVNavigationTypeLastCore = CWVNavigationTypeNewWindow,
  CWVNavigationTypeCoreMask = 0xFF,

  // Qualifiers
  // Any of the core values above can be augmented by one or more qualifiers.
  // These qualifiers further define the transition.

  // A managed user attempted to visit a URL but was blocked.
  CWVNavigationTypeBlocked = 0x00800000,

  // User used the Forward or Back button to navigate among browsing history.
  CWVNavigationTypeForwardBack = 0x01000000,

  // User used the address bar to trigger this navigation.
  CWVNavigationTypeFromAddressBar = 0x02000000,

  // User is navigating to the home page.
  CWVNavigationTypeHomePage = 0x04000000,

  // The transition originated from an external application; the exact
  // definition of this is embedder dependent.
  CWVNavigationTypeFromApi = 0x08000000,

  // The beginning of a navigation chain.
  CWVNavigationTypeChainStart = 0x10000000,

  // The last transition in a redirect chain.
  CWVNavigationTypeChainEnd = 0x20000000,

  // Redirects caused by JavaScript or a meta refresh tag on the page.
  CWVNavigationTypeClientRedirect = 0x40000000,

  // Redirects sent from the server by HTTP headers. It might be nice to
  // break this out into 2 types in the future, permanent or temporary, if we
  // can get that information from WebKit.
  CWVNavigationTypeServerRedirect = 0x80000000,

  // Used to test whether a transition involves a redirect.
  CWVNavigationTypeIsRedirectMask = 0xC0000000,

  // General mask defining the bits used for the qualifiers.
  CWVNavigationTypeQualifierMask = 0xFFFFFF00,
};

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_NAVIGATION_TYPE_H_
