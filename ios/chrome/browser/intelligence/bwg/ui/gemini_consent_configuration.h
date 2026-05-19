// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Model representing a custom header in the Gemini consent.
@interface GeminiConsentHeader : NSObject

// The icon displayed in the header.
@property(nonatomic, strong, readonly) UIImage* icon;
// The attributed title displayed in the header.
@property(nonatomic, copy, readonly) NSAttributedString* title;

// Designated initializer.
- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSAttributedString*)title
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Model representing a row in the Gemini consent accordion view.
@interface GeminiConsentRow : NSObject

// Icon displayed to the left. Always present.
@property(nonatomic, strong, readonly) UIImage* icon;
// Title displayed regardless of the collapsed state.
@property(nonatomic, copy, readonly) NSString* title;
// Body text that gets shown or hidden based on the collapsed state.
@property(nonatomic, copy, readonly) NSAttributedString* body;
// Whether the current item is collapsed and its body is visible.
@property(nonatomic, assign) BOOL collapsed;

// Designated initializer for a row, defaulting `collapsed` to `YES`.
- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
                        body:(NSAttributedString*)body
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Model with the data needed to render the Gemini consent.
@interface GeminiConsentConfiguration : NSObject

// The list of GeminiConsentRow models to be displayed in the accordion view.
@property(nonatomic, copy, readonly) NSArray<GeminiConsentRow*>* rows;
// The footnote text with styled links. Can be nil if no footnote is needed.
@property(nonatomic, copy, readonly) NSAttributedString* footnote;
// The custom header model. Can be nil if no custom header is needed.
@property(nonatomic, strong, readonly) GeminiConsentHeader* header;
// Whether the consent rows are collapsible.
@property(nonatomic, assign, readonly) BOOL collapsible;

// Designated initializer.
- (instancetype)initWithRows:(NSArray<GeminiConsentRow*>*)rows
                    footnote:(NSAttributedString*)footnote
                      header:(GeminiConsentHeader*)header
                 collapsible:(BOOL)collapsible NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_CONFIGURATION_H_
