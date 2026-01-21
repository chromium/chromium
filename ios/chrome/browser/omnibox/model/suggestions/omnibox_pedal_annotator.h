// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_OMNIBOX_PEDAL_ANNOTATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_OMNIBOX_PEDAL_ANNOTATOR_H_

#import <UIKit/UIKit.h>

struct AutocompleteMatch;
@protocol BrowserCoordinatorCommands;
@class OmniboxPedalData;
@protocol QuickDeleteCommands;
@protocol SceneCommands;
@protocol SettingsCommands;

/// A class to add pedal data to a given autocomplete match object
@interface OmniboxPedalAnnotator : NSObject

/// The endpoint that handles Actions and Pedals scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

/// The endpoint that handles Actions and Pedals settings commands.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

/// The endpoint that handles BrowserCoordinator commands.
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorHandler;

/// The endpoint that handles QuickDelete commands.
@property(nonatomic, weak) id<QuickDeleteCommands> quickDeleteHandler;

/// Creates a new pedal for the provided match.
- (OmniboxPedalData*)pedalForMatch:(const AutocompleteMatch&)match;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_OMNIBOX_PEDAL_ANNOTATOR_H_
