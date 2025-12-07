// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_COORDINATOR_DEFAULT_BROWSER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_COORDINATOR_DEFAULT_BROWSER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol ContentSuggestionsViewControllerAudience;
@class DefaultBrowserConfig;
@protocol SettingsCommands;
enum class ContentSuggestionsModuleType;
class PrefService;

// Delegate used to communicate events back to the owner of the
// DefaultBrowserMediator.
@protocol DefaultBrowserDelegate

// Indicates that Default Browser module should be removed. `completion` is
// called after the removal is finished.
- (void)removeDefaultBrowserPromoModuleWithCompletion:
    (ProceduralBlock)completion;

@end

// Mediator for managing the state of the Default Browser Magic Stack module.
@interface DefaultBrowserMediator : NSObject

// Used by the Default Browser module for the module config.
// Strong property?
@property(nonatomic, strong) DefaultBrowserConfig* config;

// Delegate.
@property(nonatomic, weak) id<DefaultBrowserDelegate> delegate;

// Audience for presentation actions.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    presentationAudience;

- (instancetype)initWithProfilePrefService:(PrefService*)profilePrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

// Removes the module from the Magic Stack without disabling the feature. This
// prevents the module from being shown on the current homepage but does not
// affect its functionality elsewhere. The `completion` is called after the
// removal is finished.
- (void)removeModuleWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_COORDINATOR_DEFAULT_BROWSER_MEDIATOR_H_
