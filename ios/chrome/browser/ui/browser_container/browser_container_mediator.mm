// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_mediator.h"

#import "base/check_op.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_consumer.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {
// Checks whether an HTTP authentication dialog is being shown by
// `overlay_presenter` for a page whose host does not match `web_state_list`'s
// active WebState's last committed URL.
bool IsActiveOverlayRequestForNonCommittedHttpAuthentication(
    WebStateList* web_state_list) {
  DCHECK(web_state_list);

  web::WebState* web_state = web_state_list->GetActiveWebState();
  if (!web_state)
    return false;

  OverlayRequest* request = OverlayRequestQueue::FromWebState(
                                web_state, OverlayModality::kWebContentArea)
                                ->front_request();
  if (!request)
    return false;

  HTTPAuthOverlayRequestConfig* config =
      request->GetConfig<HTTPAuthOverlayRequestConfig>();
  if (!config)
    return false;

  return config->url().host() != web_state->GetLastCommittedURL().host();
}
}  // namespace

@interface BrowserContainerMediator () <OverlayPresenterObserving> {
  // Observer that listens for HTTP authentication dialog presentation.
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;
  std::unique_ptr<
      base::ScopedObservation<OverlayPresenter, OverlayPresenterObserver>>
      _scopedOverlayPresenterObservation;
}
// The Browser's WebStateList.
@property(nonatomic, readonly) WebStateList* webStateList;
// Whether an HTTP authentication dialog is displayed for a page whose host does
// not match the active WebState's last committed URL.
@property(nonatomic, readonly, getter=isShowingAuthDialogForNonCommittedURL)
    BOOL showingAuthDialogForNonCommittedURL;
@end

@implementation BrowserContainerMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
      webContentAreaOverlayPresenter:(OverlayPresenter*)overlayPresenter {
  if ((self = [super init])) {
    DCHECK(overlayPresenter);
    DCHECK_EQ(overlayPresenter->GetModality(),
              OverlayModality::kWebContentArea);
    _webStateList = webStateList;
    DCHECK(_webStateList);
    // Begin observing the OverlayPresenter.
    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
    _scopedOverlayPresenterObservation = std::make_unique<
        base::ScopedObservation<OverlayPresenter, OverlayPresenterObserver>>(
        _overlayPresenterObserver.get());
    _scopedOverlayPresenterObservation->Observe(overlayPresenter);
    // Check whether an HTTP authentication dialog is shown for a page that does
    // not match the rendered contents.
    _showingAuthDialogForNonCommittedURL =
        IsActiveOverlayRequestForNonCommittedHttpAuthentication(_webStateList);
  }
  return self;
}

#pragma mark - Accessors

- (void)setConsumer:(id<BrowserContainerConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self updateConsumer];
}

- (void)setShowingAuthDialogForNonCommittedURL:
    (BOOL)showingAuthDialogForNonCommittedURL {
  if (_showingAuthDialogForNonCommittedURL ==
      showingAuthDialogForNonCommittedURL) {
    return;
  }
  _showingAuthDialogForNonCommittedURL = showingAuthDialogForNonCommittedURL;
  [self updateConsumer];
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  self.showingAuthDialogForNonCommittedURL =
      IsActiveOverlayRequestForNonCommittedHttpAuthentication(
          self.webStateList);
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.showingAuthDialogForNonCommittedURL = NO;
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  _scopedOverlayPresenterObservation = nullptr;
  _overlayPresenterObserver = nullptr;
}

#pragma mark - Private

// Updates the consumer based on the current state of the mediator.
- (void)updateConsumer {
  if (!self.consumer)
    return;
  [self.consumer setContentBlocked:self.showingAuthDialogForNonCommittedURL];
}

@end
