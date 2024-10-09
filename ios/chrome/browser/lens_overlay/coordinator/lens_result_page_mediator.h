// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_web_provider.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_bottom_sheet_presentation_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_result_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_mutator.h"
#import "ios/web/public/web_state.h"

@protocol ApplicationCommands;
@class ContextMenuConfigurationProvider;
@protocol LensResultPageConsumer;
@protocol LensResultPageMediatorDelegate;
@protocol SnackbarCommands;
class WebStateList;

/// Mediator that handles lens result page operations.
@interface LensResultPageMediator
    : NSObject <ContextMenuConfigurationProviderDelegate,
                LensOverlayResultConsumer,
                LensResultPageMutator,
                LensWebProvider>

@property(nonatomic, weak) id<LensResultPageConsumer> consumer;

/// Application commands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;
/// Snackbar commands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

/// Container for the web view.
@property(nonatomic, weak) UIView* webViewContainer;

/// Delegate for the result page web state.
@property(nonatomic, weak) id<LensResultPageMediatorDelegate> delegate;

/// Presentation delegate for requesting bottom sheet resizing.
@property(nonatomic, weak) id<LensOverlayBottomSheetPresentationDelegate>
    presentationDelegate;

/// WebState context menu configuration provider.
@property(nonatomic, weak)
    ContextMenuConfigurationProvider* contextMenuProvider;

- (instancetype)
     initWithWebStateParams:(const web::WebState::CreateParams&)params
    browserWebStateDelegate:(web::WebStateDelegate*)browserWebStateDelegate
               webStateList:(WebStateList*)webStateList
                isIncognito:(BOOL)isIncognito NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Releases managed objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_H_
