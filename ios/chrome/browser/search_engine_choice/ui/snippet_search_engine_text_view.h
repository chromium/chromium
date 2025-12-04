// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SNIPPET_SEARCH_ENGINE_TEXT_VIEW_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SNIPPET_SEARCH_ENGINE_TEXT_VIEW_H_

#import <UIKit/UIKit.h>

enum class SearchEngineCurrentDefaultState;

// View to display all text information related to one search engine, in this
// order:
//  * potentially a "default" tag (see `SearchEngineCurrentDefaultState`).
//  * the search engine name
//  * the snippet (expandable)
@interface SnippetSearchEngineTextView : UIView

// The search engine name.
@property(nonatomic, copy) NSString* searchEngineName;
// The search engine snippet for the description.
@property(nonatomic, copy) NSString* snippetText;
// Returns YES if `snippetText` doesn't fit on one line only.
@property(nonatomic, assign, readonly) BOOL isSnippetExpandable;
// Whether the snippet is shown on one line or multiple lines.
@property(nonatomic, assign) BOOL snippetExpanded;
// Anchor on Y axis centered on the text when the snippet is collapsed on one
// line (even if the snippet is expanded).
@property(nonatomic, strong, readonly)
    NSLayoutYAxisAnchor* oneLineCenterVerticalLayoutGuide;
// Layout dimension for the search engine name label height.
@property(nonatomic, strong, readonly)
    NSLayoutDimension* searchEngineNameLabelHeight;
@property(nonatomic, assign, readonly)
    SearchEngineCurrentDefaultState currentDefaultState;

// Initializes view based on `currentDefaultState`.
// SearchEngineCurrentDefaultState::kIsDefault:
//   * "Current default" is visible. The view height is based on the search
//     engine name, the snippet text and the current default tag.
// SearchEngineCurrentDefaultState::kOtherIsDefault:
//   * "Current default" is not visible. The view height is the same than
//     the view with `kIsDefault`. The search engine name and the snippet text
//     are vertically centered with extra space above and bellow.
// SearchEngineCurrentDefaultState::kNone:
//   * "Current default" is not visible. The view height is only based on
//     the search engine name and the snippet text.
- (instancetype)initWithCurrentDefaultState:
    (SearchEngineCurrentDefaultState)currentDefaultState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SNIPPET_SEARCH_ENGINE_TEXT_VIEW_H_
