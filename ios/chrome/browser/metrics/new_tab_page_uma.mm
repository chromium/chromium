// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/new_tab_page_uma.h"

#include "base/metrics/histogram_macros.h"
#include "components/google/core/common/google_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace new_tab_page_uma {

bool IsCurrentlyOnNTP(ios::ChromeBrowserState* browser_state) {
  WebStateList* webStateList =
      TabModelList::GetLastActiveTabModelForChromeBrowserState(browser_state)
          .webStateList;
  return webStateList->GetActiveWebState() &&
         webStateList->GetActiveWebState()->GetVisibleURL() ==
             kChromeUINewTabURL;
}

void RecordAction(ios::ChromeBrowserState* browserState, ActionType type) {
  DCHECK(browserState);
  if (!IsCurrentlyOnNTP(browserState) || browserState->IsOffTheRecord())
    return;
  base::HistogramBase* counter = base::Histogram::FactoryGet(
      "NewTabPage.ActioniOS", 0, NUM_ACTION_TYPES, NUM_ACTION_TYPES + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(type);
}

void RecordActionFromOmnibox(ios::ChromeBrowserState* browserState,
                             const GURL& url,
                             ui::PageTransition transition,
                             bool isExpectingVoiceSearch) {
  if (isExpectingVoiceSearch) {
    RecordAction(browserState, ACTION_NAVIGATED_USING_VOICE_SEARCH);
    return;
  }
  ui::PageTransition coreTransition = static_cast<ui::PageTransition>(
      transition & ui::PAGE_TRANSITION_CORE_MASK);
  if (PageTransitionCoreTypeIs(coreTransition, ui::PAGE_TRANSITION_GENERATED)) {
    RecordAction(browserState, ACTION_SEARCHED_USING_OMNIBOX);
  } else {
    if (google_util::IsGoogleHomePageUrl(GURL(url))) {
      RecordAction(browserState, ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE);
    } else {
      RecordAction(browserState, ACTION_NAVIGATED_USING_OMNIBOX);
    }
  }
}
}
