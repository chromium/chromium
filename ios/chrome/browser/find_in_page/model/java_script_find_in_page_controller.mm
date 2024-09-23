// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/java_script_find_in_page_controller.h"

#import <UIKit/UIKit.h>

#import <cmath>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/find_in_page/model/constants.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_response_delegate.h"
#import "ios/web/public/find_in_page/find_in_page_manager_delegate_bridge.h"
#import "ios/web/public/find_in_page/java_script_find_in_page_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"

namespace {
// Keeps find in page search term to be shared between different tabs. Never
// reset, not stored on disk.
static NSString* gSearchTerm;

// Accessibility announcement delay, so VoiceOver does not cancel the context
// string announcement when a new match has been selected.
// TODO(crbug.com/40249260): This is a temporary workaround. The context string
// announcement might still fail. A retry mechanism needs to be implemented.
const int64_t kContextStringAnnouncementDelayInNanoseconds = 0.1 * NSEC_PER_SEC;
}  // namespace

@interface JavaScriptFindInPageController () <CRWFindInPageManagerDelegate>

// The web view's scroll view.
- (CRWWebViewScrollViewProxy*)webViewScrollView;
// Find in Page text field listeners.
- (void)findBarTextFieldWillBecomeFirstResponder:(NSNotification*)note;
- (void)findBarTextFieldDidResignFirstResponder:(NSNotification*)note;
// Keyboard listeners.
- (void)keyboardDidShow:(NSNotification*)note;
- (void)keyboardWillHide:(NSNotification*)note;
// Records UKM metric for Find in Page search matches.
- (void)logFindInPageSearchUKM;
// Prevent scrolling past the end of the page.
- (CGPoint)limitOverscroll:(CRWWebViewScrollViewProxy*)scrollViewProxy
                   atPoint:(CGPoint)point;
@end

@implementation JavaScriptFindInPageController {
  // Object that manages searches and match traversals.
  raw_ptr<web::JavaScriptFindInPageManager> _findInPageManager;

  // Access to the web view from the web state.
  id<CRWWebViewProxy> _webViewProxy;

  // True when a find is in progress. Used to avoid running JavaScript during
  // disable when there is nothing to clear.
  BOOL _findStringStarted;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  raw_ptr<web::WebState> _webState;

  // Bridge to observe FindInPageManager from Objective-C.
  std::unique_ptr<web::FindInPageManagerDelegateBridge>
      _findInPageDelegateBridge;
}

@synthesize findInPageModel = _findInPageModel;

+ (void)setSearchTerm:(NSString*)string {
  gSearchTerm = [string copy];
}

+ (NSString*)searchTerm {
  return gSearchTerm;
}

- (id)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    DCHECK(webState->IsRealized());

    _webState = webState;
    _findInPageModel = [[FindInPageModel alloc] init];
    _findInPageDelegateBridge =
        std::make_unique<web::FindInPageManagerDelegateBridge>(self);
    _findInPageManager =
        web::JavaScriptFindInPageManager::FromWebState(_webState);
    _findInPageManager->SetDelegate(_findInPageDelegateBridge.get());

    _webViewProxy = _webState->GetWebViewProxy();
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(findBarTextFieldWillBecomeFirstResponder:)
               name:kFindBarTextFieldWillBecomeFirstResponderNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(findBarTextFieldDidResignFirstResponder:)
               name:kFindBarTextFieldDidResignFirstResponderNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_webState) << "-detachFromWebState must be called before -dealloc";
}

- (BOOL)canFindInPage {
  return _findInPageManager->CanSearchContent();
}

- (CRWWebViewScrollViewProxy*)webViewScrollView {
  return [_webViewProxy scrollViewProxy];
}

- (CGPoint)limitOverscroll:(CRWWebViewScrollViewProxy*)scrollViewProxy
                   atPoint:(CGPoint)point {
  CGFloat contentHeight = scrollViewProxy.contentSize.height;
  CGFloat frameHeight = scrollViewProxy.frame.size.height;
  CGFloat maxScroll = std::max<CGFloat>(0, contentHeight - frameHeight);
  if (point.y > maxScroll) {
    point.y = maxScroll;
  }
  return point;
}

- (void)logFindInPageSearchUKM {
  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(_webState);
  if (sourceID != ukm::kInvalidSourceId) {
    ukm::builders::IOS_FindInPageSearchMatches(sourceID)
        .SetHasMatches(_findInPageModel.matches > 0)
        .Record(ukm::UkmRecorder::Get());
  }
}

- (void)findStringInPage:(NSString*)query {
  // Keep track of whether a find is in progress so to avoid running
  // JavaScript during disable if unnecessary.
  _findStringStarted = YES;
  // Save the query in the model before searching. TODO:(crbug.com/963908):
  // Remove as part of refactoring.
  [self.findInPageModel updateQuery:query matches:0];
  _findInPageManager->Find(query, web::FindInPageOptions::FindInPageSearch);
}

- (void)findNextStringInPage {
  _findInPageManager->Find(nil, web::FindInPageOptions::FindInPageNext);
}

// Highlight the previous search match, update model and scroll to match.
- (void)findPreviousStringInPage {
  _findInPageManager->Find(nil, web::FindInPageOptions::FindInPagePrevious);
}

// Remove highlights from the page and disable the model.
- (void)disableFindInPage {
  if (![self canFindInPage]) {
    return;
  }

  // Only run FindInPageManager::StopFinding() if there is a string in progress
  // to avoid WKWebView crash on deallocation due to outstanding completion
  // handler.
  if (_findStringStarted) {
    _findInPageManager->StopFinding();
    _findStringStarted = NO;
  }
}

- (void)saveSearchTerm {
  [[self class] setSearchTerm:[[self findInPageModel] text]];
}

- (void)restoreSearchTerm {
  // Pasteboards always return nil in background:
  if ([[UIApplication sharedApplication] applicationState] !=
      UIApplicationStateActive) {
    return;
  }

  NSString* term = [[self class] searchTerm];
  [[self findInPageModel] updateQuery:(term ? term : @"") matches:0];
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
}

- (void)findInPageManager:(web::AbstractFindInPageManager*)manager
    didSelectMatchAtIndex:(NSInteger)index
        withContextString:(NSString*)contextString
              forWebState:(web::WebState*)webState {
  if (contextString) {
    // TODO(crbug.com/40249260): When tapping the Previous or Next button in the
    // Find Bar, VoiceOver will trigger the announcement of the title of the
    // button, usually a fraction of a second after this method is called. As a
    // result, the announcement triggered by the
    // `UIAccessibilityAnnouncementNotification` posted here will be interrupted
    // by the announcement of the button. Setting a delay on posting the context
    // string announcement notification yields the opposite result i.e. the
    // expected result: VoiceOver will not read "Previous" or "Next", but read
    // the new context string instead. This is a temporary workaround. The
    // context string announcement might still fail. Some kind of retry
    // mechanism needs to be implemented.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kContextStringAnnouncementDelayInNanoseconds),
                   dispatch_get_main_queue(), ^{
                     UIAccessibilityPostNotification(
                         UIAccessibilityAnnouncementNotification,
                         contextString);
                   });
  }
  // Increment index so that match number show in FindBar ranges from 1...N as
  // opposed to 0...N-1.
  index++;
  [self.findInPageModel updateIndex:index atPoint:CGPointZero];
  [self.responseDelegate findDidFinishWithUpdatedModel:self.findInPageModel];
}

- (void)userDismissedFindNavigatorForManager:
    (web::AbstractFindInPageManager*)manager {
  // There should not be any Find navigator in JavaScript Find in Page.
  NOTREACHED_IN_MIGRATION();
}

#pragma mark - Notification listeners

- (void)findBarTextFieldWillBecomeFirstResponder:(NSNotification*)note {
  // Listen to the keyboard appearance notifications.
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(keyboardDidShow:)
                        name:UIKeyboardDidShowNotification
                      object:nil];
  [defaultCenter addObserver:self
                    selector:@selector(keyboardWillHide:)
                        name:UIKeyboardWillHideNotification
                      object:nil];
}

- (void)findBarTextFieldDidResignFirstResponder:(NSNotification*)note {
  // Resign from the keyboard appearance notifications on the next turn of the
  // runloop.
  dispatch_async(dispatch_get_main_queue(), ^{
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter removeObserver:self
                             name:UIKeyboardDidShowNotification
                           object:nil];
    [defaultCenter removeObserver:self
                             name:UIKeyboardWillHideNotification
                           object:nil];
  });
}

- (void)keyboardDidShow:(NSNotification*)note {
  NSDictionary* info = [note userInfo];
  CGSize kbSize =
      [[info objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue].size;
  CGFloat kbHeight = kbSize.height;
  UIEdgeInsets insets = UIEdgeInsetsZero;
  insets.bottom = kbHeight;
  [_webViewProxy registerInsets:insets forCaller:self];
}

- (void)keyboardWillHide:(NSNotification*)note {
  [_webViewProxy unregisterInsetsForCaller:self];
}

- (void)detachFromWebState {
  _findInPageManager->SetDelegate(nullptr);
  _findInPageDelegateBridge.reset();
  _findInPageManager = nullptr;
  _webState = nullptr;
}

@end
