// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_navigation_type.h"

#include <ostream>

#include "base/notreached.h"
#import "ios/web_view/internal/cwv_navigation_type_internal.h"

CWVNavigationType CWVNavigationTypeFromPageTransition(
    ui::PageTransition ui_page_transition) {
  CWVNavigationType cwv_navigation_type = 0;

  ui::PageTransition core_transition =
      ui::PageTransitionStripQualifier(ui_page_transition);
  switch (core_transition) {
    case ui::PAGE_TRANSITION_LINK:
      cwv_navigation_type = CWVNavigationTypeLink;
      break;
    case ui::PAGE_TRANSITION_TYPED:
      cwv_navigation_type = CWVNavigationTypeTyped;
      break;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      cwv_navigation_type = CWVNavigationTypeAutoBookmark;
      break;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      cwv_navigation_type = CWVNavigationTypeAutoSubframe;
      break;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      cwv_navigation_type = CWVNavigationTypeManualSubframe;
      break;
    case ui::PAGE_TRANSITION_GENERATED:
      cwv_navigation_type = CWVNavigationTypeGenerated;
      break;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      cwv_navigation_type = CWVNavigationTypeAutoToplevel;
      break;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      cwv_navigation_type = CWVNavigationTypeFormSubmit;
      break;
    case ui::PAGE_TRANSITION_RELOAD:
      cwv_navigation_type = CWVNavigationTypeReload;
      break;
    case ui::PAGE_TRANSITION_KEYWORD:
      cwv_navigation_type = CWVNavigationTypeKeyword;
      break;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      cwv_navigation_type = CWVNavigationTypeKeywordGenerated;
      break;
    default:
      // The compiler cannot do this check automatically because
      // ui::PageTransition contains both core values and qualifiers while only
      // core values are enumerated here.
      NOTREACHED_IN_MIGRATION()
          << "Unknown core value of ui::PageTransition. Update "
             "CWVNavigationTypeFromUIPageTransition() to add one.";
  }

  if (ui_page_transition & ui::PAGE_TRANSITION_BLOCKED) {
    cwv_navigation_type |= CWVNavigationTypeBlocked;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    cwv_navigation_type |= CWVNavigationTypeForwardBack;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) {
    cwv_navigation_type |= CWVNavigationTypeFromAddressBar;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_HOME_PAGE) {
    cwv_navigation_type |= CWVNavigationTypeHomePage;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_FROM_API) {
    cwv_navigation_type |= CWVNavigationTypeFromApi;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_CHAIN_START) {
    cwv_navigation_type |= CWVNavigationTypeChainStart;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_CHAIN_END) {
    cwv_navigation_type |= CWVNavigationTypeChainEnd;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    cwv_navigation_type |= CWVNavigationTypeClientRedirect;
  }
  if (ui_page_transition & ui::PAGE_TRANSITION_SERVER_REDIRECT) {
    cwv_navigation_type |= CWVNavigationTypeServerRedirect;
  }

  return cwv_navigation_type;
}
