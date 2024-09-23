// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_CONSTRAINTS_UI_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_CONSTRAINTS_UI_UTIL_H_

#import <UIKit/UIKit.h>
#import <type_traits>

// A bitmask to refer to sides of a layout rectangle.
enum class LayoutSides {
  kTop = 1 << 0,
  kLeading = 1 << 1,
  kBottom = 1 << 2,
  kTrailing = 1 << 3,
};

// Implementation of bitwise "or", "and" operators (as those are not
// automatically defined for "class enum").
constexpr LayoutSides operator|(LayoutSides lhs, LayoutSides rhs) {
  return static_cast<LayoutSides>(
      static_cast<std::underlying_type<LayoutSides>::type>(lhs) |
      static_cast<std::underlying_type<LayoutSides>::type>(rhs));
}

constexpr LayoutSides operator&(LayoutSides lhs, LayoutSides rhs) {
  return static_cast<LayoutSides>(
      static_cast<std::underlying_type<LayoutSides>::type>(lhs) &
      static_cast<std::underlying_type<LayoutSides>::type>(rhs));
}

// Returns whether the `flag` is set in `mask`.
constexpr bool IsLayoutSidesMaskSet(LayoutSides mask, LayoutSides flag) {
  return (mask & flag) == flag;
}

// Defines a protocol for the edge anchor methods of UIView and UILayoutGuide.
@protocol EdgeLayoutGuideProvider <NSObject>
@property(nonatomic, readonly, strong) NSLayoutXAxisAnchor* leadingAnchor;
@property(nonatomic, readonly, strong) NSLayoutXAxisAnchor* trailingAnchor;
@property(nonatomic, readonly, strong) NSLayoutYAxisAnchor* topAnchor;
@property(nonatomic, readonly, strong) NSLayoutYAxisAnchor* bottomAnchor;
@end

// ComposedEdgeLayoutGuide is a layout guide based on the anchors of
// multiple views. It is useful when the details of how the layout guide is
// created shouldn't be exposed.
@interface ComposedEdgeLayoutGuide : NSObject <EdgeLayoutGuideProvider>
// A base layout guide to serve the anchors not defined with a provider.
@property(nonatomic, weak) id<EdgeLayoutGuideProvider> baseLayoutGuide;
// Each of the edge anchors can have a different provider assigned. If none is
// assigned, the respective `baseLayoutGuide` anchor will be returned.
@property(nonatomic, weak) id<EdgeLayoutGuideProvider> leadingAnchorProvider;
@property(nonatomic, weak) id<EdgeLayoutGuideProvider> trailingAnchorProvider;
@property(nonatomic, weak) id<EdgeLayoutGuideProvider> topAnchorProvider;
@property(nonatomic, weak) id<EdgeLayoutGuideProvider> bottomAnchorProvider;
@end

// Defines a protocol for common -...Anchor methods of UIView and UILayoutGuide.
@protocol LayoutGuideProvider <EdgeLayoutGuideProvider>
@property(nonatomic, readonly, strong) NSLayoutXAxisAnchor* leftAnchor;
@property(nonatomic, readonly, strong) NSLayoutXAxisAnchor* rightAnchor;
@property(nonatomic, readonly, strong) NSLayoutDimension* widthAnchor;
@property(nonatomic, readonly, strong) NSLayoutDimension* heightAnchor;
@property(nonatomic, readonly, strong) NSLayoutXAxisAnchor* centerXAnchor;
@property(nonatomic, readonly, strong) NSLayoutYAxisAnchor* centerYAnchor;
@end

// UIView already supports the methods in LayoutGuideProvider.
@interface UIView (LayoutGuideProvider) <LayoutGuideProvider>
@end

// UILayoutGuide already supports the methods in LayoutGuideProvider.
@interface UILayoutGuide (LayoutGuideProvider) <LayoutGuideProvider>
@end

#pragma mark - Visual constraints.

// Applies all `constraints` to views in `subviewsDictionary`.
void ApplyVisualConstraints(NSArray* constraints,
                            NSDictionary* subviewsDictionary);

// Applies all `constraints` with `metrics` to views in `subviewsDictionary`.
void ApplyVisualConstraintsWithMetrics(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSDictionary* metrics);

// Applies all `constraints` with `metrics` and `options` to views in
// `subviewsDictionary`.
void ApplyVisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options);

// Returns constraints based on the visual constraints described with
// `constraints` and `metrics` to views in `subviewsDictionary`.
NSArray* VisualConstraintsWithMetrics(NSArray* constraints,
                                      NSDictionary* subviewsDictionary,
                                      NSDictionary* metrics);

// Returns constraints based on the visual constraints described with
// `constraints`, `metrics` and `options` to views in `subviewsDictionary`.
NSArray* VisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options);

#pragma mark - Constraints between two views.
// Most methods in this group can take a layout guide or a view.

// Adds a constraint that `view1` and `view2` are center-aligned horizontally
// and vertically.
void AddSameCenterConstraints(id<LayoutGuideProvider> view1,
                              id<LayoutGuideProvider> view2);

// Adds a constraint that `view1` and `view2` are center-aligned horizontally.
// `view1` and `view2` must be in the same view hierarchy.
void AddSameCenterXConstraint(id<LayoutGuideProvider> view1,
                              id<LayoutGuideProvider> view2);
// Deprecated version:
void AddSameCenterXConstraint(UIView* unused_parentView,
                              id<LayoutGuideProvider> subview1,
                              id<LayoutGuideProvider> subview2);

// Adds a constraint that `view1` and `view2` are center-aligned vertically.
// `view1` and `view2` must be in the same view hierarchy.
void AddSameCenterYConstraint(id<LayoutGuideProvider> view1,
                              id<LayoutGuideProvider> view2);
// Deprecated version:
void AddSameCenterYConstraint(UIView* unused_parentView,
                              id<LayoutGuideProvider> subview1,
                              id<LayoutGuideProvider> subview2);

// Adds constraints to make two views' size and center equal by pinning leading,
// trailing, top and bottom anchors.
void AddSameConstraints(id<EdgeLayoutGuideProvider> view1,
                        id<EdgeLayoutGuideProvider> view2);

// Constraints all sides of `innerView` and `outerView` together, with
// `innerView` inset by `insets`.
void AddSameConstraintsWithInsets(id<EdgeLayoutGuideProvider> innerView,
                                  id<EdgeLayoutGuideProvider> outerView,
                                  NSDirectionalEdgeInsets insets);

// Constraints all sides of `innerView` and `outerView` together, with
// `innerView` inset by `inset` on all sides.
void AddSameConstraintsWithInset(id<EdgeLayoutGuideProvider> innerView,
                                 id<EdgeLayoutGuideProvider> outerView,
                                 CGFloat inset);

// Adds constraints to make `innerView` leading, trailing, top and bottom
// anchors equals to `outerView` safe area (or view bounds) anchors.
void PinToSafeArea(id<EdgeLayoutGuideProvider> innerView, UIView* outerView);

// Constraints `side_flags` of `view1` and `view2` together.
// Example usage: AddSameConstraintsToSides(view1, view2,
// LayoutSides::kTop|LayoutSides::kLeading)
void AddSameConstraintsToSides(id<EdgeLayoutGuideProvider> view1,
                               id<EdgeLayoutGuideProvider> view2,
                               LayoutSides side_flags);

// Constraints `side_flags` sides of `innerView` and `outerView` together, with
// `innerView` inset by `insets`. Example usage:
// AddSameConstraintsToSidesWithInsets(view1, view2,
// LayoutSides::kTop|LayoutSides::kLeading, NSDirectionalEdgeInsets{10, 5,
// 10, 5}) - This will constraint innerView to be inside of outerView, with
// leading/trailing inset by 10 and top/bottom inset by 5.
// Edge insets for sides not listed in `side_flags` are ignored.
void AddSameConstraintsToSidesWithInsets(id<EdgeLayoutGuideProvider> innerView,
                                         id<EdgeLayoutGuideProvider> outerView,
                                         LayoutSides side_flags,
                                         NSDirectionalEdgeInsets insets);

// Constraints the width and height of `view` to the given `size`.
void AddSizeConstraints(id<LayoutGuideProvider> view, CGSize size);

// Constraints the width and height of `view` to the given `edge` of a square.
void AddSquareConstraints(id<LayoutGuideProvider> view, CGFloat edge);

// Adds an optional amount of padding to the top and bottom of a view using a
// constraint with a lowered priority. One use case is with a collectionview or
// tableview cell. When the cell is self-sizing, these constraints will kick in
// and expand the cell to add the desired padding around the inner views, but
// the padding is optional so that the inner views are not artificially
// shortened when fixed-size cells cut into that padding.  The padding is added
// between `outerView` and `innerView`.
// Returns the top and bottom layouts that have been created.
NSArray<NSLayoutConstraint*>* AddOptionalVerticalPadding(
    id<EdgeLayoutGuideProvider> outerView,
    id<EdgeLayoutGuideProvider> innerView,
    CGFloat padding);
NSArray<NSLayoutConstraint*>* AddOptionalVerticalPadding(
    id<EdgeLayoutGuideProvider> outerView,
    id<EdgeLayoutGuideProvider> topInnerView,
    id<EdgeLayoutGuideProvider> bottomInnerView,
    CGFloat padding);

// Returns the vertical constraint of `innerView` and `outerView`. The height of
// `outerView` equals to the height of `innerView` plus `inset`.
NSLayoutConstraint* VerticalConstraintsWithInset(UIView* innerView,
                                                 UIView* outerView,
                                                 CGFloat inset);

#pragma mark - Safe Area.

#endif  // IOS_CHROME_COMMON_UI_UTIL_CONSTRAINTS_UI_UTIL_H_
