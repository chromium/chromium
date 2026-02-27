// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class AssistantContainerDetent;
@protocol AssistantContainerDelegate;

// View Controller for the Assistant Container.
@interface AssistantContainerViewController : UIViewController

// Whether the container is currently being animated by an external animator.
@property(nonatomic, assign) BOOL isAnimating;

// The view to anchor to. If nil, falls back to the bottom of the parent view.
@property(nonatomic, weak) UIView* anchorView;

// Whether to anchor to the bottom of the view (YES) or the top (NO).
// Defaults to NO.
@property(nonatomic, assign) BOOL anchorToBottom;

// The available detents for the container.
@property(nonatomic, strong) NSArray<AssistantContainerDetent*>* detents;

// The delegate for the container events.
@property(nonatomic, weak) id<AssistantContainerDelegate> delegate;

// Default initializer.
- (instancetype)initWithViewController:(UIViewController*)viewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_CONTROLLER_H_
