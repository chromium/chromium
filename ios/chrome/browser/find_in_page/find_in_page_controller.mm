// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"

#import <UIKit/UIKit.h>

#import "base/i18n/number_formatting.h"
#import "components/strings/grit/components_strings.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/find_in_page_response_delegate.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/public/provider/chrome/browser/find_in_page/find_in_page_api.h"
#import "ios/web/public/find_in_page/find_in_page_manager.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate_bridge.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Keeps find in page search term to be shared between different tabs. Never
// reset, not stored on disk.
NSString* gSearchTerm;
// Accessibility announcement delay, so VoiceOver does not cancel the context
// string announcement when a new match has been selected.
// TODO(crbug.com/1395828): This is a temporary workaround. The context string
// announcement might still fail. A retry mechanism needs to be implemented.
constexpr int64_t kResultsAnnouncementDelayInNanoseconds = 0.1 * NSEC_PER_SEC;
}  // namespace

@interface FindInPageController () <CRWFindInPageManagerDelegate>

// Records UKM metric for Find in Page search matches.
- (void)logFindInPageSearchUKM;

@end

@implementation FindInPageController {
  // Object that manages searches and match traversals.
  web::FindInPageManager* _findInPageManager;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;

  // Bridge to observe FindInPageManager from Objective-C.
  std::unique_ptr<web::FindInPageManagerDelegateBridge>
      _findInPageDelegateBridge;
}

+ (void)clearSearchTerm {
  gSearchTerm = nil;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    DCHECK(webState->IsRealized());

    // A Find interaction should be used iff the current variant of Native Find
    // in Page is the System Find panel variant.
    const bool useFindInteraction =
        ios::provider::IsNativeFindInPageWithSystemFindPanel();
    // The Find in Page manager should not be attached yet.
    DCHECK(!web::FindInPageManager::FromWebState(webState));
    web::FindInPageManager::CreateForWebState(webState, useFindInteraction);

    _webState = webState;
    _findInPageModel = [[FindInPageModel alloc] init];
    _findInPageDelegateBridge =
        std::make_unique<web::FindInPageManagerDelegateBridge>(self);
    _findInPageManager = web::FindInPageManager::FromWebState(_webState);
    _findInPageManager->SetDelegate(_findInPageDelegateBridge.get());
  }
  return self;
}

- (void)findStringInPage:(NSString*)query {
  [self.findInPageModel updateQuery:query matches:0];
  _findInPageManager->Find(query, web::FindInPageOptions::FindInPageSearch);
}

- (void)findNextStringInPage {
  _findInPageManager->Find(nil, web::FindInPageOptions::FindInPageNext);
}

- (void)findPreviousStringInPage {
  _findInPageManager->Find(nil, web::FindInPageOptions::FindInPagePrevious);
}

- (void)disableFindInPage {
  _findInPageManager->StopFinding();
}

- (BOOL)canFindInPage {
  NewTabPageTabHelper* ntpHelper = NewTabPageTabHelper::FromWebState(_webState);
  BOOL isNTPActive = ntpHelper->IsActive();
  return !isNTPActive && _findInPageManager->CanSearchContent();
}

- (void)saveSearchTerm {
  gSearchTerm = [self.findInPageModel.text copy];
}

- (void)restoreSearchTerm {
  [self.findInPageModel updateQuery:gSearchTerm matches:0];
}

#pragma mark - Private

- (void)logFindInPageSearchUKM {
  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(_webState);
  if (sourceID != ukm::kInvalidSourceId) {
    ukm::builders::IOS_FindInPageSearchMatches(sourceID)
        .SetHasMatches(_findInPageModel.matches > 0)
        .Record(ukm::UkmRecorder::Get());
  }
}

#pragma mark - CRWFindInPageManagerDelegate

- (void)findInPageManager:(web::AbstractFindInPageManager*)manager
    didHighlightMatchesOfQuery:(NSString*)query
                withMatchCount:(NSInteger)matchCount
                   forWebState:(web::WebState*)webState {
  if (matchCount == 0 && !query) {
    // StopFinding responds with `matchCount` as 0 and `query` as nil.
    [self.responseDelegate findDidStop];
    [self logFindInPageSearchUKM];
    return;
  }
  [self.findInPageModel updateQuery:query matches:matchCount];
  [self.responseDelegate findDidFinishWithUpdatedModel:self.findInPageModel];

  // Announce there are no results with Voiceover if
  // 1. Native Find in Page uses the Chrome Find Bar UI,
  // 2. search finished with 0 matches,
  // 3. and the query was not empty.
  if (ios::provider::IsNativeFindInPageWithChromeFindBar() && matchCount == 0 &&
      query.length > 0) {
    NSString* resultsCountAnnouncementString =
        l10n_util::GetNSString(IDS_ACCESSIBLE_FIND_IN_PAGE_NO_RESULTS);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kResultsAnnouncementDelayInNanoseconds),
                   dispatch_get_main_queue(), ^{
                     UIAccessibilityPostNotification(
                         UIAccessibilityAnnouncementNotification,
                         resultsCountAnnouncementString);
                   });
  }
}

- (void)findInPageManager:(web::AbstractFindInPageManager*)manager
    didSelectMatchAtIndex:(NSInteger)index
        withContextString:(NSString*)contextString
              forWebState:(web::WebState*)webState {
  // In the model, indices start at 1, hence `index + 1`.
  [self.findInPageModel updateIndex:index + 1 atPoint:CGPointZero];
  [self.responseDelegate findDidFinishWithUpdatedModel:self.findInPageModel];

  if (ios::provider::IsNativeFindInPageWithChromeFindBar()) {
    // If Native Find in Page uses the Chrome Find Bar UI, announce the new
    // selected match with Voiceover.
    NSString* resultsCountAnnouncementString = l10n_util::GetNSStringF(
        IDS_ACCESSIBLE_FIND_IN_PAGE_COUNT,
        base::FormatNumber(self.findInPageModel.currentIndex),
        base::FormatNumber(self.findInPageModel.matches));
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kResultsAnnouncementDelayInNanoseconds),
                   dispatch_get_main_queue(), ^{
                     UIAccessibilityPostNotification(
                         UIAccessibilityAnnouncementNotification,
                         resultsCountAnnouncementString);
                   });
  }
}

- (void)userDismissedFindNavigatorForManager:
    (web::AbstractFindInPageManager*)manager {
  DCHECK(ios::provider::IsNativeFindInPageWithSystemFindPanel());
  // User dismissed the Find panel so mark the Find UI as inactive.
  self.findInPageModel.enabled = NO;
}

- (void)detachFromWebState {
  _findInPageManager->SetDelegate(nullptr);
  _findInPageManager = nullptr;
  _findInPageDelegateBridge.reset();
  // Remove Find in Page manager from web state.
  web::FindInPageManager::RemoveFromWebState(_webState);
  _webState = nullptr;
}

- (void)dealloc {
  DCHECK(!_webState) << "-detachFromWebState must be called before -dealloc";
}

@end
