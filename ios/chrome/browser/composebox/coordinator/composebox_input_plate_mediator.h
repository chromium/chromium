// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <vector>

#import "components/contextual_search/internal/ios/composebox_context_upload_observer_bridge.h"
#import "components/contextual_tasks/public/query_contextualizer.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_omnibox_client_delegate.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_tab_picker_coordinator.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_consumer.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_mutator.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"

@protocol BrowserCoordinatorCommands;
@class CobrowseContext;
class CobrowseBrowserAgent;
@protocol ComposeboxDebuggerLogger;
@class ComposeboxMetricsRecorder;
@protocol ComposeboxURLLoader;
@protocol SceneCommands;
enum class FuseboxAttachmentButtonType;
class AimEligibilityService;
class FaviconLoader;
class PersistTabContextBrowserAgent;
class PrefService;
class ProfileIOS;
class TemplateURLService;
class WebStateList;

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

@protocol ComposeboxInputPlateMediatorDelegate
// Reloads the composebox autocomplete suggestions.
- (void)reloadAutocompleteSuggestionsRestarting:(BOOL)restart;
// Refines the query with the given `text`.
- (void)refineWithText:(NSString*)text;
// Informs the delegate that adding an attachment failed due to limit.
- (void)showAttachmentLimitError;
// Informs the delegate that item upload has failed.
- (void)showSnackbarForItemUploadDidFail;
@end

// Mediator for the composebox composebox.
@interface ComposeboxInputPlateMediator
    : NSObject <ComposeboxOmniboxClientDelegate,
                ComposeboxInputPlateMutator,
                ComposeboxContextUploadObserver,
                ComposeboxModeObserver,
                ComposeboxTabPickerSelectionDelegate,
                TextFieldViewContainingHeightDelegate,
                VoiceSearchDelegate>

// The composebox input plate consumer.
@property(nonatomic, weak) id<ComposeboxInputPlateConsumer> consumer;
// The composebox URL loader.
@property(nonatomic, weak) id<ComposeboxURLLoader> URLLoader;
// The delegate for this mediator.
@property(nonatomic, weak) id<ComposeboxInputPlateMediatorDelegate> delegate;
// The metrics recorder of the composebox.
@property(nonatomic, weak) ComposeboxMetricsRecorder* metricsRecorder;
// Delegate for logging events.
@property(nonatomic, weak) id<ComposeboxDebuggerLogger> debugLogger;

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
                        prefService:(PrefService*)prefService
                            profile:(ProfileIOS*)profile
               cobrowseBrowserAgent:(CobrowseBrowserAgent*)cobrowseBrowserAgent
          browserCoordinatorHandler:
              (id<BrowserCoordinatorCommands>)browserCoordinatorHandler
                       sceneHandler:(id<SceneCommands>)sceneHandler
                         entrypoint:(ComposeboxEntrypoint)entrypoint;

- (void)disconnect;

// Returns whether more attachments can be added.
- (BOOL)canAddMoreAttachments;

// Returns the maximum number of attachments allowed based on the current
// composebox mode and current number of attachments.
- (NSUInteger)remainingAttachmentCapacity;

// Returns the maximum number of images allowed based on the current
// composebox mode and current number of attachments.
- (NSUInteger)remainingNumberOfImagesAllowed;

// Records that the plus menu opened with the given visible attachment buttons,
// and maps dynamically injected Tools and Models to metrics.
- (void)recordPlusMenuOpenedWithVisibleInternalButtons:
    (const std::vector<FuseboxAttachmentButtonType>&)visibleInternalButtons;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_
