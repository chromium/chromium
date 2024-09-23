// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_mediator.h"
#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface TextZoomCoordinator () <ContainedPresenterDelegate>

// The view controller managed by this coordinator.
@property(nonatomic, strong, readwrite)
    TextZoomViewController* textZoomViewController;

@property(nonatomic, strong) TextZoomMediator* mediator;

// Allows simplified access to the TextZoomCommands handler.
@property(nonatomic) id<TextZoomCommands> textZoomCommandHandler;

@end

@implementation TextZoomCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);

  self.textZoomCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TextZoomCommands);

  self.mediator = [[TextZoomMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
            commandHandler:self.textZoomCommandHandler];

  self.textZoomViewController = [[TextZoomViewController alloc] init];
  self.textZoomViewController.commandHandler = self.textZoomCommandHandler;

  self.textZoomViewController.zoomHandler = self.mediator;
  self.mediator.consumer = self.textZoomViewController;

  DCHECK(self.currentWebState);
  FontSizeTabHelper* helper =
      FontSizeTabHelper::FromWebState(self.currentWebState);
  // If Text Zoom UI is already active, just reshow it
  if (helper->IsTextZoomUIActive()) {
    [self showAnimated:NO];
  } else {
    helper->SetTextZoomUIActive(true);
    [self showAnimated:YES];
  }
}

- (void)stop {
  if (![self.presenter
          isPresentingViewController:self.textZoomViewController]) {
    return;
  }
  // If the Text Zoom UI is still active, the dismiss should be unanimated,
  // because the UI will be brought back later.
  BOOL animated;
  if (self.currentWebState) {
    FontSizeTabHelper* helper =
        FontSizeTabHelper::FromWebState(self.currentWebState);
    animated = helper && !helper->IsTextZoomUIActive();
  } else {
    animated = YES;
  }

  [self.presenter dismissAnimated:animated];
  self.textZoomViewController = nil;

  [self.mediator disconnect];
  self.mediator.consumer = nil;
  self.mediator = nil;
}

- (void)showAnimated:(BOOL)animated {
  self.presenter.presentedViewController = self.textZoomViewController;
  self.presenter.delegate = self;

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:animated];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  [self.delegate toolbarAccessoryCoordinatorDidDismissUI:self];
}

#pragma mark - Private

- (web::WebState*)currentWebState {
  return self.browser->GetWebStateList()
             ? self.browser->GetWebStateList()->GetActiveWebState()
             : nullptr;
}

@end
