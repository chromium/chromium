// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_STATE_MANAGER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_STATE_MANAGER_H_

#import <Foundation/Foundation.h>

#import <map>
#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "components/contextual_search/input_state_model.h"
#import "components/lens/lens_overlay_mime_type.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "third_party/omnibox_proto/searchbox_config.pb.h"
#import "third_party/omnibox_proto/tool_mode.pb.h"

class AimEligibilityService;
class PrefService;
class WebStateList;

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace signin {
class IdentityManager;
}  // namespace signin

@class ComposeboxInputStateManager;

// Delegate protocol for ComposeboxInputStateManager.
@protocol ComposeboxInputStateManagerDelegate <NSObject>

// Called when the input state is updated.
- (void)inputStateManager:(ComposeboxInputStateManager*)manager
      didUpdateInputState:(const contextual_search::InputState&)inputState;

@end

// Manages the state and metrics for the composebox input.
@interface ComposeboxInputStateManager : NSObject

// The delegate to be notified of state updates.
@property(nonatomic, weak) id<ComposeboxInputStateManagerDelegate> delegate;

// The current active tool mode.
@property(nonatomic, assign) omnibox::ToolMode activeTool;

// The current active model option. Use `setActiveModel:explicitUserAction:` to
// set the model.
@property(nonatomic, readonly) ComposeboxModelOption activeModel;

// The current input state.
@property(nonatomic, readonly) const contextual_search::InputState& inputState;

// Initializes the manager.
- (instancetype)
     initWithWebStateList:(WebStateList*)webStateList
              prefService:(PrefService*)prefService
    aimEligibilityService:(AimEligibilityService*)aimEligibilityService
          identityManager:(signin::IdentityManager*)identityManager
            sessionHandle:
                (contextual_search::ContextualSearchSessionHandle*)sessionHandle
               entrypoint:(ComposeboxEntrypoint)entrypoint
              isIncognito:(BOOL)isIncognito NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the manager and cleans up resources.
- (void)disconnect;

// Sets the searchbox configuration.
- (void)setSearchboxConfig:(const omnibox::SearchboxConfig&)searchboxConfig;

// Sets the active model. If explicitUserAction is YES, records metrics.
- (void)setActiveModel:(ComposeboxModelOption)modelOption
    explicitUserAction:(BOOL)explicitUserAction;

// Retrieves additional query parameters from the underlying input state model.
- (std::map<std::string, std::string>)additionalQueryParams;

// Notifies the model that the context has changed.
- (void)onContextChanged;

// Records the active modes when a submission occurs.
- (void)recordInputStateOnSubmission;

// Records the active modes and file types at the end of the session.
- (void)
    recordInputStateOnSessionEndWithNavigation:(BOOL)inNavigation
                                     mimeTypes:
                                         (const std::vector<lens::MimeType>&)
                                             mimeTypes;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_STATE_MANAGER_H_
