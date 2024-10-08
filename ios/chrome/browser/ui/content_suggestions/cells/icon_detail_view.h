// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ICON_DETAIL_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ICON_DETAIL_VIEW_H_

#import <UIKit/UIKit.h>

/// TODO(crbug.com/371968237): Refactor Icon Detail View to reduce the number
/// of parameters passed to the initializer.
@class IconDetailView;

// A protocol for handling `IconDetailView` taps. `-didTapIconDetailView:view`
// will be called when an `IconDetailView` is tapped.
@protocol IconDetailViewTapDelegate

// Indicates that the user has tapped the given `view`.
- (void)didTapIconDetailView:(IconDetailView*)view;

@end

// The possible layout types for a given `IconDetailView`. These values
// determine how the content within the `IconDetailView` is arranged.
enum class IconDetailViewLayoutType {
  // A prominent layout with a larger icon and more spacing between
  // the title and description. Suitable for single-row displays.
  kHero = 1,
  // A more condensed layout with a smaller icon and less spacing
  // between the title and description. Useful for multi-row views.
  kCompact = 2,
  kMaxValue = kCompact
};

// A view to display an icon, title, description, and (optional) chevron. This
// view can be configured with different layout types to suit various display
// needs.
@interface IconDetailView : UIView

// Initializer for creating an `IconDetailView` with the
// given `title`, `description`, `layoutType`, `symbolName`,
// `symbolColorPalette`, `symbolBackgroundColor` (and whether it
// `usesDefaultSymbol`), `symbolwidth`, and `accessibilityIdentifier`. This
// initializer provides a streamlined way to set up an `IconDetailView`
// instance. When `showCheckmark` is true, the icon is displayed with a green
// checkmark to indicate a completed state.
// The icon will display a badge based on
// `badgeSymbolName`, `badgeUsesDefaultSymbol`, and `badgeBackgroundColor`.
- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
           symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
        symbolBackgroundColor:(UIColor*)symbolBackgroundColor
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                  symbolWidth:(CGFloat)symbolwidth
                showCheckmark:(BOOL)showCheckmark
              badgeSymbolName:(NSString*)badgeSymbolName
         badgeBackgroundColor:(UIColor*)badgeBackgroundColor
       badgeUsesDefaultSymbol:(BOOL)badgeUsesDefaultSymbol
      accessibilityIdentifier:(NSString*)accessibilityIdentifier;

// Initializer for creating an `IconDetailView` with the
// given `title`, `description`, `layoutType`, `symbolName`,
// `symbolColorPalette`, `symbolBackgroundColor` (and whether it
// `usesDefaultSymbol`), and `accessibilityIdentifier`. This initializer
// provides a streamlined way to set up an `IconDetailView` instance. When
// `showCheckmark` is true, the icon is displayed with a green checkmark to
// indicate a completed state.
// The icon will display a badge based on
// `badgeSymbolName`, `badgeUsesDefaultSymbol`, and `badgeBackgroundColor`.
- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
           symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
        symbolBackgroundColor:(UIColor*)symbolBackgroundColor
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                showCheckmark:(BOOL)showCheckmark
              badgeSymbolName:(NSString*)badgeSymbolName
         badgeBackgroundColor:(UIColor*)badgeBackgroundColor
       badgeUsesDefaultSymbol:(BOOL)badgeUsesDefaultSymbol
      accessibilityIdentifier:(NSString*)accessibilityIdentifier;

// Initializer for creating an `IconDetailView` with the
// given `title`, `description`, `layoutType`, `symbolName`,
// `symbolColorPalette`, `symbolBackgroundColor` (and whether it
// `usesDefaultSymbol`), and `accessibilityIdentifier`. This initializer
// provides a streamlined way to set up an `IconDetailView` instance. When
// `showCheckmark` is true, the icon is displayed with a green checkmark to
// indicate a completed state.
- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
           symbolColorPalette:(NSArray<UIColor*>*)symbolColorPalette
        symbolBackgroundColor:(UIColor*)symbolBackgroundColor
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                showCheckmark:(BOOL)showCheckmark
      accessibilityIdentifier:(NSString*)accessibilityIdentifier;

// Initializer for creating an `IconDetailView` with the
// given `title`, `description`, `layoutType`, `symbolName` (and whether it
// `usesDefaultSymbol`), `symbolWidth`, and `accessibilityIdentifier`.
// This initializer provides a streamlined way to set up an
// `IconDetailView` instance. When `showCheckmark` is true, the icon is
// displayed with a green checkmark to indicate a completed state.
- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                  symbolWidth:(CGFloat)symbolWidth
                showCheckmark:(BOOL)showCheckmark
      accessibilityIdentifier:(NSString*)accessibilityIdentifier;

// Initializer for creating an `IconDetailView` with the
// given `title`, `description`, `layoutType`, `symbolName` (and whether it
// `usesDefaultSymbol`), and `accessibilityIdentifier`. This initializer
// provides a streamlined way to set up an `IconDetailView` instance. When
// `showCheckmark` is true, the icon is displayed with a green checkmark to
// indicate a completed state.
- (instancetype)initWithTitle:(NSString*)title
                  description:(NSString*)description
                   layoutType:(IconDetailViewLayoutType)layoutType
                   symbolName:(NSString*)symbolName
            usesDefaultSymbol:(BOOL)usesDefaultSymbol
                showCheckmark:(BOOL)showCheckmark
      accessibilityIdentifier:(NSString*)accessibilityIdentifier;

// The object that should receive a message when this view is tapped.
@property(nonatomic, weak) id<IconDetailViewTapDelegate> tapDelegate;

// Unique identifier for the item. Can be `nil`.
@property(nonatomic, copy) NSString* identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_ICON_DETAIL_VIEW_H_
