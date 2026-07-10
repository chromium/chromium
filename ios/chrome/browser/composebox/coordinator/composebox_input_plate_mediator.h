// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <set>
#include <vector>

#import "components/contextual_search/internal/ios/composebox_context_upload_observer_bridge.h"
#import "components/contextual_tasks/public/query_contextualizer.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_omnibox_client_delegate.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_consumer.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_mutator.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_controller.h"
#import "ios/web/public/web_state_id.h"

@protocol BrowserCoordinatorCommands;
@class ComposeboxAttachmentSelection;
@class ComposeboxFocusParams;
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

namespace base {
class UnguessableToken;
}  // namespace base

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
    : NSObject <ComposeboxContextUploadObserver,
                ComposeboxInputPlateMutator,
                ComposeboxInputStateManagerDelegate,
                ComposeboxOmniboxClientDelegate,
                TextFieldViewContainingHeightDelegate,
                VoiceSearchDelegate>

// The composebox input plate consumer.
@property(nonatomic, weak) id<ComposeboxInputPlateConsumer> consumer;
// The current real-time attachment selection.
@property(nonatomic, readonly)
    ComposeboxAttachmentSelection* currentAttachmentSelection;
// The current computed UI input state.
@property(nonatomic, readonly) ComposeboxUIInputState* currentUIInputState;
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
            (const std::vector<FuseboxAttachmentButtonType>&)
                visibleInternalButtons
                                          uiInputState:
                                              (ComposeboxUIInputState*)state;

// Unpacks and attaches all items within the selection wrapper.
- (void)updateAttachments:(ComposeboxAttachmentSelection*)attachments;

// Removes the shared tab with the given `serverToken`.
- (void)removeSharedTabWithServerToken:
    (const base::UnguessableToken&)serverToken;

// Applies the focus parameters to initialize the session state.
- (void)applyFocusParams:(ComposeboxFocusParams*)params;

// Processes the picked Google Drive file metadata and triggers contextual
// session upload.
- (void)processDriveFileWithIdentifier:(NSString*)identifier
                                  name:(NSString*)name
                              mimeType:(NSString*)mimeType;

// Returns the associated IDs for all currently attached tabs.
- (std::set<web::WebStateID>)allAttachedWebStateIDs;

// Returns the associated IDs for currently attached tabs from the current web
// state context. Tabs attached from different web states (not visible in the
// tab picker) will be excluded.
- (std::set<web::WebStateID>)attachedWebStateIDsInCurrentContext;

// Returns the maximum number of tab attachments allowed.
- (NSUInteger)maxTabAttachmentCount;

// Attaches the selected tabs.
- (void)attachSelectedTabsWithWebStateIDs:
            (std::set<web::WebStateID>)selectedWebStateIDs
                        cachedWebStateIDs:
                            (std::set<web::WebStateID>)cachedWebStateIDs;

// Processes a webpage context from a context library signal. Called on the
// cobrowse context only.
- (void)processContextLibraryWebpageSignalWithURL:(const GURL&)url
                                            title:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_H_
