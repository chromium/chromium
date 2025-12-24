// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_omnibox_client_delegate.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_tab_picker_coordinator.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_consumer.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_mutator.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"

@class ComposeboxMetricsRecorder;
@protocol ComposeboxURLLoader;
class AimEligibilityService;
class FaviconLoader;
class GURL;
class PersistTabContextBrowserAgent;
class PrefService;
class TemplateURLService;
class WebStateList;

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

// Delegate for the ComposeboxInputPlateMediator.
@protocol ComposeboxInputPlateMediatorDelegate
// Reloads the composebox autocomplete suggestions.
- (void)reloadAutocompleteSuggestionsRestarting:(BOOL)restart;
// Informs the delegate that adding an attachment failed due to limit.
- (void)showAttachmentLimitError;
// Informs the delegate that item upload has failed.
- (void)showSnackbarForItemUploadDidFail;
@end

// Mediator for the composebox composebox.
@interface ComposeboxInputPlateMediator
    : NSObject <ComposeboxOmniboxClientDelegate,
                ComposeboxInputPlateMutator,
                ComposeboxFileUploadObserver,
                ComposeboxModeObserver,
                ComposeboxTabPickerSelectionDelegate,
                LoadQueryCommands,
                TextFieldViewContainingHeightDelegate>

@property(nonatomic, weak) id<ComposeboxInputPlateConsumer> consumer;
@property(nonatomic, weak) id<ComposeboxURLLoader> URLLoader;
// The delegate for this mediator.
@property(nonatomic, weak) id<ComposeboxInputPlateMediatorDelegate> delegate;
// The metrics recorder of the composebox.
@property(nonatomic, weak) ComposeboxMetricsRecorder* metricsRecorder;

- (instancetype)
    initWithContextualSearchSession:
        (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)
            contextualSearchSession
                       webStateList:(WebStateList*)webStateList
                      faviconLoader:(FaviconLoader*)faviconLoader
             persistTabContextAgent:
                 (PersistTabContextBrowserAgent*)persistTabContextAgent
                        isIncognito:(BOOL)isIncognito
                         modeHolder:(ComposeboxModeHolder*)modeHolder
                 templateURLService:(TemplateURLService*)templateURLService
              aimEligibilityService:
                  (AimEligibilityService*)aimEligibilityService
                        prefService:(PrefService*)prefService;

- (void)disconnect;

// Processes the given `itemProvider` for an image.
- (void)processImageItemProvider:(NSItemProvider*)itemProvider
                         assetID:(NSString*)assetID;

// Processes the given `PDFFileURL` for a file.
- (void)processPDFFileURL:(GURL)PDFFileURL;

// Returns whether more attachments can be added.
- (BOOL)canAddMoreAttachments;

// Returns the maximum number of gallery items allowed based on the current
// composebox mode.
- (NSUInteger)maxNumberOfGalleryItemsAllowed;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_
