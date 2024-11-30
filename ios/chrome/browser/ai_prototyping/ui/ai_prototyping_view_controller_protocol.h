// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_PROTOCOL_H_

@protocol AIPrototypingMutator;

// The shared properties of each page in the menu.
@protocol AIPrototypingViewControllerProtocol

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<AIPrototypingMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_VIEW_CONTROLLER_PROTOCOL_H_
