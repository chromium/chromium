// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SNIPPET_SEARCH_ENGINE_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SNIPPET_SEARCH_ENGINE_BUTTON_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@class FaviconView;

// State of the snippet in SnippetSearchEngineButton.
enum class SnippetButtonState {
  // The chevron is pointing down, the snippet label shows one line.
  kOneLine,
  // The chevron is pointing up, the snippet label shows all the text.
  kExpanded,
};

// Button used in the search engine choice to display one search engine.
// This button contains a search engine title, a snippet, a radio button, and
// a chevron.
// By default the snippet first line is only displayed. When the chevron is
// tapped the button handles the animation, and trigger `layoutIfNeeded` call on
// `animatedLayoutView` to have smooth animation.
// The owner should use `-[SnippetSearchEngineButton addTarget:action:
// forControlEvents:]` to receive the tap event.
@interface SnippetSearchEngineButton : UIControl

// Favicon image to display the search engine icon.
@property(nonatomic, strong) UIImage* faviconImage;
// The search engine name.
@property(nonatomic, copy) NSString* searchEngineName;
// The search engine snippet for the description.
@property(nonatomic, copy) NSString* snippetText;
// Snippet state (hidden or closed). Setting the value using this property
// will not trigger animation.
@property(nonatomic, assign) SnippetButtonState snippetButtonState;
// YES if the search engine has been selected by the user.
@property(nonatomic, assign) BOOL checked;
// Identifier for button.
@property(nonatomic, copy) NSString* searchEngineKeyword;
// View to layout when animating the chevron. This should be a weak pointer
// to avoid a circular retain cycle, since it should be a super view of `self`.
@property(nonatomic, weak) UIView* animatedLayoutView;
// YES to hide an horizontal separator at the button of the button.
// Default value is NO.
// The horizontal separator should be hidden for the last button of a stack
// view.
@property(nonatomic, assign) BOOL horizontalSeparatorHidden;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Reconfigure the accessibility traits according to the current state of the
// cell.
- (void)updateAccessibilityTraits;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SNIPPET_SEARCH_ENGINE_BUTTON_H_
