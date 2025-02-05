// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_PROTOCOL_H_

@protocol AIPrototypingMutator;

enum class AIPrototypingFeature : NSInteger;

// The shared properties of each page in the menu.
@protocol AIPrototypingViewControllerProtocol

// The feature related to this page of the prototyping menu.
@property(nonatomic, readonly) AIPrototypingFeature feature;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<AIPrototypingMutator> mutator;

// Initializes the view controller for a given `feature`.
- (instancetype)initForFeature:(AIPrototypingFeature)feature;

// Updates the page's response field with `response`.
- (void)updateResponseField:(NSString*)response;

// Enable submit buttons, and style them accordingly.
- (void)enableSubmitButtons;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_PROTOCOL_H_
