// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_ACCORDION_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_ACCORDION_VIEW_H_

#import <UIKit/UIKit.h>

// A model representing an item in the accordion.
@interface GeminiConsentRow : NSObject

// Icon displayed to the left. Always present.
@property(nonatomic, strong, readonly) UIImage* icon;
// Title displayed regardless of the collapsed state.
@property(nonatomic, copy, readonly) NSString* title;
// Body text that gets shown or hidden based on the collapsed state.
@property(nonatomic, copy, readonly) NSAttributedString* body;
// Whether the current item is collapsed and its body is visible.
@property(nonatomic, assign) BOOL collapsed;

- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
                        body:(NSAttributedString*)body
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@class GeminiConsentAccordionView;

// Delegate protocol to handle user interactions in the accordion view.
@protocol GeminiConsentAccordionViewDelegate <NSObject>

// Called whenever an URL is selected inside an item's body
- (void)accordionView:(GeminiConsentAccordionView*)view didTapLink:(NSURL*)url;
// Called whenever an accordion item collapsed/expanded state changes.
- (void)accordionView:(GeminiConsentAccordionView*)view
         didToggleRow:(GeminiConsentRow*)row;

@end

// A self-contained view that displays a list of collapsible items.
//
// When `collapsible` is true, the entire view is tappable in order to toggle
// between the collapsed and expanded state. Otherwise, [v] is omitted and the
//  view doesn't have a tap interaction.
//
// This is the layout for each items added to the view (`GeminiConsentRowView`):
// +--------------------------+
// | [*] | Title        | [v] |
// |     | Body...      |     |
// +--------------------------+
// [*] = Icon, [v] = Chevron (optional)
@interface GeminiConsentAccordionView : UIView

// The delegate to handle interactions.
@property(nonatomic, weak) id<GeminiConsentAccordionViewDelegate> delegate;

// Initializes the view with a list of rows and a `collapsible` parameter that
// controls the ability to collapse and expand elements in the view.
- (instancetype)initWithRows:(NSArray<GeminiConsentRow*>*)rows
                 collapsible:(BOOL)collapsible NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_ACCORDION_VIEW_H_
