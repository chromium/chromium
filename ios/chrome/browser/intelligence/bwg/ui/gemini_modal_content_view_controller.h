// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_MODAL_CONTENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_MODAL_CONTENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class GeminiModalContentViewController;

// Delegate for user-initiated dismissal of the modal.
@protocol GeminiModalContentViewControllerDelegate <NSObject>

// Called when the user taps the close button. The delegate is responsible for
// dismissing `viewController` as this class never dismisses itself.
- (void)geminiModalContentViewControllerDidTapClose:
    (GeminiModalContentViewController*)viewController;

@end

// Hosts arbitrary content in a modal underneath a navigation bar with a close
// button. The owner is responsible for supplying the content, keeping `title`
// up to date, and dismissal.
@interface GeminiModalContentViewController : UIViewController

// Notified when the user taps the close button.
@property(nonatomic, weak) id<GeminiModalContentViewControllerDelegate>
    delegate;

// Initializes the view controller with the view rendering the content.
- (instancetype)initWithContentView:(UIView*)contentView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibName
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_MODAL_CONTENT_VIEW_CONTROLLER_H_
