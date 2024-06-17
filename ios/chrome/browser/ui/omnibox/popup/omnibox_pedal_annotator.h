// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_PEDAL_ANNOTATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_PEDAL_ANNOTATOR_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
struct AutocompleteMatch;
@protocol OmniboxCommands;
@class OmniboxPedalData;
@protocol QuickDeleteCommands;
@protocol SettingsCommands;

/// A class to add pedal data to a given autocomplete match object
@interface OmniboxPedalAnnotator : NSObject

/// The endpoint that handles Actions and Pedals application commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

/// The endpoint that handles Actions and Pedals settings commands.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

/// The endpoint that handles Omnibox commands.
@property(nonatomic, weak) id<OmniboxCommands> omniboxHandler;

/// The endpoint that handles QuickDelete commands.
@property(nonatomic, weak) id<QuickDeleteCommands> quickDeleteHandler;

/// Creates a new pedal for the provided match.
- (OmniboxPedalData*)pedalForMatch:(const AutocompleteMatch&)match;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_PEDAL_ANNOTATOR_H_
