// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_OMNIBOX_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_OMNIBOX_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/model/omnibox_text_controller_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_mutator.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_view_controller.h"

class FaviconLoader;
@protocol LensCommands;
@protocol LoadQueryCommands;
@protocol OmniboxCommands;
@protocol OmniboxConsumer;
@protocol OmniboxMediatorDelegate;
@class OmniboxTextController;
@class SceneState;
class TemplateURLService;
class UrlLoadingBrowserAgent;

namespace feature_engagement {
class Tracker;
}

/// A mediator object that updates the omnibox according to the model changes.
@interface OmniboxMediator : NSObject <OmniboxMutator,
                                       OmniboxTextControllerDelegate,
                                       OmniboxViewControllerPasteDelegate>

/// Delegate for events in this class.
@property(nonatomic, weak) id<OmniboxMediatorDelegate> delegate;

/// Controller of the omnibox text.
@property(nonatomic, weak) OmniboxTextController* omniboxTextController;

/// The templateURLService used by this mediator to extract whether the default
/// search engine supports search-by-image.
@property(nonatomic, assign) TemplateURLService* templateURLService;

/// The `URLLoadingBrowserAgent` used by this mediator to start search-by-image.
@property(nonatomic, assign) UrlLoadingBrowserAgent* URLLoadingBrowserAgent;

/// The consumer for this object. This can change during the lifetime of this
/// object and may be nil.
@property(nonatomic, weak) id<OmniboxConsumer> consumer;

@property(nonatomic, weak) id<LoadQueryCommands> loadQueryCommandsHandler;
@property(nonatomic, weak) id<LensCommands> lensCommandsHandler;
@property(nonatomic, weak) id<OmniboxCommands> omniboxCommandsHandler;

/// The favicon loader.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

/// Scene state used by this mediator to log with
/// NonModalDefaultBrowserPromoSchedulerSceneAgent.
@property(nonatomic, weak) SceneState* sceneState;

- (instancetype)initWithIncognito:(BOOL)isIncognito
                          tracker:(feature_engagement::Tracker*)tracker
                    isLensOverlay:(BOOL)isLensOverlay NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Sets the image used in image search.
- (void)setThumbnailImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_OMNIBOX_MEDIATOR_H_
