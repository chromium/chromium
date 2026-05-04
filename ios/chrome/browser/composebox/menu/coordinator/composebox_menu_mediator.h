// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_mutator.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
#import "ios/web/public/web_state_id.h"

@class ComposeboxAttachmentSelection;
@class ComposeboxMenuMediator;
@class ComposeboxUIInputState;
@class ComposeboxPickerImageResult;
@protocol ComposeboxMenuConsumer;

// Delegate for the menu mediator.
@protocol ComposeboxMenuMediatorDelegate <NSObject>

// Called when a tool option is tapped.
- (void)composeboxMenuMediator:(ComposeboxMenuMediator*)mediator
                    didTapTool:(ComposeboxMode)toolMode;

// Called when a model option is tapped.
- (void)composeboxMenuMediator:(ComposeboxMenuMediator*)mediator
                   didTapModel:(ComposeboxModelOption)modelMode;

// Called when the picked attachments are updated.
- (void)composeboxMenuMediator:(ComposeboxMenuMediator*)mediator
          didUpdateAttachments:(ComposeboxAttachmentSelection*)attachments;

// Called when the camera selection is requested.
- (void)composeboxMenuMediatorDidRequestCameraSelection:
    (ComposeboxMenuMediator*)mediator;

// Called when the gallery selection is requested.
- (void)composeboxMenuMediatorDidRequestGallerySelection:
    (ComposeboxMenuMediator*)mediator;

// Called when the file selection is requested.
- (void)composeboxMenuMediatorDidRequestFileSelection:
    (ComposeboxMenuMediator*)mediator;

// Called when the tab selection is requested.
- (void)composeboxMenuMediatorDidRequestTabSelection:
    (ComposeboxMenuMediator*)mediator;

@end

// Mediator for the composebox menu.
@interface ComposeboxMenuMediator : NSObject <ComposeboxMenuMutator>

// Delegate for this mediator.
@property(nonatomic, weak) id<ComposeboxMenuMediatorDelegate> delegate;

// Consumer for this mediator.
@property(nonatomic, weak) id<ComposeboxMenuConsumer> consumer;

// Creates a new instance with an entrypoint and the initial UI state.
- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint
                        inputState:(ComposeboxUIInputState*)inputState;

/// Processes the given `imageItems`.
- (void)processImageItems:(NSArray<ComposeboxPickerImageResult*>*)imageItems;

/// Processes the given `urls`.
- (void)processFileURLs:(NSArray<NSURL*>*)urls;

/// Processes the given web state IDs.
- (void)processWebStateIDs:(std::set<web::WebStateID>)selectedWebStateIDs
         cachedWebStateIDs:(std::set<web::WebStateID>)cachedWebStateIDs;

/// Returns whether more attachments can be added.
- (BOOL)canAddMoreAttachments;

// Returns the maximum number of images allowed based on the current
// composebox mode and current number of attachments.
- (NSUInteger)remainingNumberOfImagesAllowed;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_MEDIATOR_H_
