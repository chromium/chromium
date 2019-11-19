// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_page_control.h"

#import <CoreGraphics/CoreGraphics.h>
#include <algorithm>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Structure of this control:
//
// The page control is similar to a UISegmentedControl in appearance, but not in
// function. This control doesn't have segments that highlight; instead there
// is a white "slider" that moves across the view onto whichever segment is
// active. Each segment has an image and (optionally) a label. When the slider
// is over a segment, the corresponding image and label are colored black-on-
// white and are slightly larger. This is implemented by having two versions of
// each image and label; the larger "selected" versions are positioned above the
// smaller ones in the view hierarchy but are masked out by the slider view, so
// they are only seen when the slider is over them.
//
// This control is built out of several views. From the (z-axis) bottom up, they
// are:
//
//  * The background view, a grey roundrect with vertical transparent bars.
//  * The background image views.
//  * The numeric label on the regular tab icon.
//  * The "slider" view -- a white roundrect that's taller and wider than each
//    of the background segments. It clips its subview to its bounds, and it
//    adjusts its subview's frame so that it (the subview) remains fixed
//    relative to the background.
//     * The selected image view, which contains the selected images and labels
//       and is a subview of the slider.
//        * The selected images and label.

// Notes on layout:
// This control has an intrinsic size, and generally ignores frame changes. It's
// not expected that its bounds will ever change.
// Given that, it's generally simpler to used fixed (frame-based) layout for
// most of the content of this control. However, in order to acommodate RTL
// layout, three layout guides are used to define the position of the
// incognito, regular, and remote tab sections. The layout frames of these
// guides are used to map points in the view to specific TabGridPage values.
// This means that the initial view layout for this control happens in two
// phases. -setupViews creates all of the subviews and the layout guides, but
// the positions of the images and labels is set in -layoutSubviews, after the
// constrainst for the guides have been applied.

namespace {
// Height and width of the slider.
const CGFloat kSliderHeight = 40.0;
const CGFloat kSliderWidth = 78.0;

// Height and width of each segment.
const CGFloat kSegmentHeight = 36.0;
const CGFloat kSegmentWidth = 64.0;

// Points that the slider overhangs a segment on each side, or 0 if the slider
// is narrower than a segment.
const CGFloat kSliderOverhang =
    std::max((kSliderWidth - kSegmentWidth) / 2.0, 0.0);

// Width of the separator bars between segments.
const CGFloat kSeparatorWidth = 1.0;

// Width of the background -- three segments plus two separators.
const CGFloat kBackgroundWidth = 3 * kSegmentWidth + 2 * kSeparatorWidth;

// Overall height of the control -- the larger of the slider and segment
// heights.
const CGFloat kOverallHeight = std::max(kSliderHeight, kSegmentHeight);
// Overall width of the control -- the background width plus twice the slider
// overhang.
const CGFloat kOverallWidth = kBackgroundWidth + 2 * kSliderOverhang;

// Radius used to draw the background and the slider.
const CGFloat kCornerRadius = 13.0;

// Sizes for the labels and their selected counterparts.
const CGFloat kLabelSize = 20.0;
const CGFloat kSelectedLabelSize = 23.0;

// Maximum duration of slider motion animation.
const NSTimeInterval kSliderMoveDuration = 0.2;

// Color for the slider
const int kSliderColor = 0xF8F9FA;
// Color for the background view.
const int kBackgroundColor = 0xFFFFFF;
// Alpha for the background view.
const CGFloat kBackgroundAlpha = 0.3;
// Color for the regular tab count label and icons.
const CGFloat kSelectedColor = 0x3C4043;

// Returns the point that's at the center of |rect|.
CGPoint RectCenter(CGRect rect) {
  return CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
}

// Convenience method that composes an asset name and returns the correct image
// (in template rendering mode) based on the segment name (one of "regular",
// "incognito, "remote") and whether the selected state image is needed or not.
UIImage* ImageForSegment(NSString* segment, BOOL selected) {
  NSString* asset =
      [NSString stringWithFormat:@"page_control_%@_tabs%@", segment,
                                 selected ? @"_selected" : @""];
  return [[UIImage imageNamed:asset]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}
}

// View class used for the background of this control; it draws the grey
// rectangles with clear separators.
@interface TabGridPageControlBackground : UIView
@end

@interface TabGridPageControl () <UIGestureRecognizerDelegate> {
  UIAccessibilityElement* _incognitoAccessibilityElement;
  UIAccessibilityElement* _regularAccessibilityElement;
  UIAccessibilityElement* _remoteAccessibilityElement;
}

// Layout guides used to position segment-specific content.
@property(nonatomic, weak) UILayoutGuide* incognitoGuide;
@property(nonatomic, weak) UILayoutGuide* regularGuide;
@property(nonatomic, weak) UILayoutGuide* remoteGuide;
// The view for the slider.
@property(nonatomic, weak) UIView* sliderView;
// The view for the selected images and labels (a subview of |sliderView).
@property(nonatomic, weak) UIView* selectedImageView;
// The labels for the incognito and regular sections, in regular and selected
// variants.
@property(nonatomic, weak) UIView* incognitoIcon;
@property(nonatomic, weak) UIView* incognitoSelectedIcon;
@property(nonatomic, weak) UIView* regularIcon;
@property(nonatomic, weak) UIView* regularSelectedIcon;
@property(nonatomic, weak) UILabel* regularLabel;
@property(nonatomic, weak) UILabel* regularSelectedLabel;

@property(nonatomic, weak) UIView* remoteIcon;
@property(nonatomic, weak) UIView* remoteSelectedIcon;
// The center point for the slider corresponding to a |sliderPosition| of 0.
@property(nonatomic) CGFloat sliderOrigin;
// The (signed) x-coordinate distance the slider moves over. The slider's
// position is set by adding a fraction of this distance to |sliderOrigin|, so
// that when |sliderRange| is negative (in RTL layout), the slider will move in
// the negative-x direction from |sliderOrigin|, and otherwise it will move in
// the positive-x direction.
@property(nonatomic) CGFloat sliderRange;
// State properties to track the point and position (in the 0.0-1.0 range) of
// drags.
@property(nonatomic) CGPoint dragStart;
@property(nonatomic) CGFloat dragStartPosition;
@end

@implementation TabGridPageControl

+ (instancetype)pageControl {
  return [[TabGridPageControl alloc] init];
}

- (instancetype)init {
  CGRect frame = CGRectMake(0, 0, kOverallWidth, kOverallHeight);
  if (self = [super initWithFrame:frame]) {
    // Default to the regular tab page as the selected page.
    _selectedPage = TabGridPageRegularTabs;

    _incognitoAccessibilityElement =
        [[UIAccessibilityElement alloc] initWithAccessibilityContainer:self];
    _incognitoAccessibilityElement.accessibilityTraits =
        UIAccessibilityTraitButton;
    _incognitoAccessibilityElement.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_INCOGNITO_TABS_TITLE);
    _incognitoAccessibilityElement.accessibilityIdentifier =
        kTabGridIncognitoTabsPageButtonIdentifier;

    _regularAccessibilityElement =
        [[UIAccessibilityElement alloc] initWithAccessibilityContainer:self];
    _regularAccessibilityElement.accessibilityTraits =
        UIAccessibilityTraitButton;
    _regularAccessibilityElement.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_REGULAR_TABS_TITLE);
    _regularAccessibilityElement.accessibilityIdentifier =
        kTabGridRegularTabsPageButtonIdentifier;

    _remoteAccessibilityElement =
        [[UIAccessibilityElement alloc] initWithAccessibilityContainer:self];
    _remoteAccessibilityElement.accessibilityTraits =
        UIAccessibilityTraitButton;
    _remoteAccessibilityElement.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_REMOTE_TABS_TITLE);
    _remoteAccessibilityElement.accessibilityIdentifier =
        kTabGridRemoteTabsPageButtonIdentifier;

    self.accessibilityElements = @[
      _incognitoAccessibilityElement, _regularAccessibilityElement,
      _remoteAccessibilityElement
    ];
  }
  return self;
}

#pragma mark - Public Properies

- (void)setSelectedPage:(TabGridPage)selectedPage {
  [self setSelectedPage:selectedPage animated:NO];
}

- (void)setSliderPosition:(CGFloat)sliderPosition {
  // Clamp |selectionOffset| to (0.0 - 1.0).
  sliderPosition = base::ClampToRange<CGFloat>(sliderPosition, 0.0, 1.0);
  CGPoint center = self.sliderView.center;
  center.x = self.sliderOrigin + self.sliderRange * sliderPosition;
  self.sliderView.center = center;
  // Reposition the selected image view so that it's still centered in the
  // control itself.
  self.selectedImageView.center =
      [self convertPoint:RectCenter(self.bounds) toView:self.sliderView];
  _sliderPosition = sliderPosition;

  // |_selectedPage| should be kept in sync with the slider position.
  TabGridPage previousSelectedPage = _selectedPage;
  if (sliderPosition < 0.25)
    _selectedPage = TabGridPageIncognitoTabs;
  else if (sliderPosition < 0.75)
    _selectedPage = TabGridPageRegularTabs;
  else
    _selectedPage = TabGridPageRemoteTabs;

  if (_selectedPage != previousSelectedPage)
    [self updateSelectedPageAccessibility];
}

// Setters for the control's text values. These need to update three things:
// the text in both labels (the regular and the  "selected" versions that's
// visible when the slider is over a segment), and an ivar to store values that
// are set before the labels are created.
- (void)setRegularTabCount:(NSUInteger)regularTabCount {
  NSString* regularText = TextForTabCount(regularTabCount);
  self.regularLabel.text = regularText;
  self.regularSelectedLabel.text = regularText;
  _regularTabCount = regularTabCount;
}

#pragma mark - Public methods

- (void)setSelectedPage:(TabGridPage)selectedPage animated:(BOOL)animated {
  CGFloat newPosition;
  switch (selectedPage) {
    case TabGridPageIncognitoTabs:
      newPosition = 0.0;
      break;
    case TabGridPageRegularTabs:
      newPosition = 0.5;
      break;
    case TabGridPageRemoteTabs:
      newPosition = 1.0;
      break;
  }
  if (self.selectedPage == selectedPage && newPosition == self.sliderPosition) {
    return;
  }

  _selectedPage = selectedPage;
  [self updateSelectedPageAccessibility];
  if (animated) {
    // Scale duration to the distance the slider travels, but cap it at
    // the slider move duration. This means that for motion induced by
    // tapping on a section, the duration will be the same even if the slider
    // is moving across two segments.
    CGFloat offsetDelta = abs(newPosition - self.sliderPosition);
    NSTimeInterval duration = offsetDelta * kSliderMoveDuration;
    [UIView animateWithDuration:std::min(duration, kSliderMoveDuration)
                     animations:^{
                       self.sliderPosition = newPosition;
                     }];
  } else {
    self.sliderPosition = newPosition;
  }
}

#pragma mark - UIControl

- (BOOL)beginTrackingWithTouch:(UITouch*)touch withEvent:(UIEvent*)event {
  CGPoint locationInSlider = [touch locationInView:self.sliderView];
  if ([self.sliderView pointInside:locationInSlider withEvent:event]) {
    self.dragStart = [touch locationInView:self];
    self.dragStartPosition = self.sliderPosition;
    return YES;
  }
  return NO;
}

- (BOOL)continueTrackingWithTouch:(UITouch*)touch withEvent:(UIEvent*)event {
  // Compute x-distance offset
  CGPoint position = [touch locationInView:self];
  CGFloat deltaX = position.x - self.dragStart.x;
  // Convert to position change.
  CGFloat positionChange = deltaX / self.sliderRange;

  self.sliderPosition = self.dragStartPosition + positionChange;
  [self sendActionsForControlEvents:UIControlEventValueChanged];
  return YES;
}

- (void)endTrackingWithTouch:(UITouch*)touch withEvent:(UIEvent*)event {
  // UIControl requires that the superclass method is called.
  [super endTrackingWithTouch:touch withEvent:event];
  [self setSelectedPage:self.selectedPage animated:YES];
  // UIControl will send actions for UIControlEventTouchUpInside as part of its
  // UIResponder implementation, so there's no need to send them at this point.
}

- (void)cancelTrackingWithEvent:(UIEvent*)event {
  [super cancelTrackingWithEvent:event];
  [self setSelectedPage:self.selectedPage animated:YES];
  // UIControl doesn't sent control events for -cancelTrackingWithEvent:, so
  // explicitly do so here.
  [self sendActionsForControlEvents:UIControlEventTouchUpInside];
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kOverallWidth, kOverallHeight);
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // The first time this moves to a superview, perform the view setup.
  if (newSuperview && self.subviews.count == 0) {
    [self setupViews];
  }
}

- (void)layoutSubviews {
  // The superclass call should be made first, so the constraint-based layout
  // guides can be set correctly.
  [super layoutSubviews];
  [self updateAccessibilityFrames];

  // Position the section images and labels, which depend on the layout guides.
  self.incognitoIcon.center = [self centerOfSegment:TabGridPageIncognitoTabs];
  self.incognitoSelectedIcon.center =
      [self centerOfSegment:TabGridPageIncognitoTabs];

  self.regularIcon.center = [self centerOfSegment:TabGridPageRegularTabs];
  self.regularSelectedIcon.center =
      [self centerOfSegment:TabGridPageRegularTabs];
  self.regularLabel.center = [self centerOfSegment:TabGridPageRegularTabs];
  self.regularSelectedLabel.center =
      [self centerOfSegment:TabGridPageRegularTabs];

  self.remoteIcon.center = [self centerOfSegment:TabGridPageRemoteTabs];
  self.remoteSelectedIcon.center = [self centerOfSegment:TabGridPageRemoteTabs];

  // Determine the slider origin and range; this is based on the layout guides
  // and can't be computed until they are determined.
  self.sliderOrigin = CGRectGetMidX(self.incognitoGuide.layoutFrame);
  self.sliderRange =
      CGRectGetMidX(self.remoteGuide.layoutFrame) - self.sliderOrigin;

  // Set the slider position using the new slider origin and range.
  self.sliderPosition = _sliderPosition;
}

#pragma mark - UIAccessibility (informal protocol)

- (BOOL)isAccessibilityElement {
  return NO;
}

#pragma mark - UIAccessibilityContainer Helpers

- (NSString*)accessibilityIdentifierForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return kTabGridIncognitoTabsPageButtonIdentifier;
    case TabGridPageRegularTabs:
      return kTabGridRegularTabsPageButtonIdentifier;
    case TabGridPageRemoteTabs:
      return kTabGridRemoteTabsPageButtonIdentifier;
  }
}

- (void)updateSelectedPageAccessibility {
  NSString* selectedPageID =
      [self accessibilityIdentifierForPage:self.selectedPage];
  for (UIAccessibilityElement* element in self.accessibilityElements) {
    element.accessibilityTraits = UIAccessibilityTraitButton;
    if ([element.accessibilityIdentifier isEqualToString:selectedPageID])
      element.accessibilityTraits |= UIAccessibilityTraitSelected;
  }
}

- (void)updateAccessibilityFrames {
  _incognitoAccessibilityElement.accessibilityFrameInContainerSpace =
      self.incognitoGuide.layoutFrame;
  _regularAccessibilityElement.accessibilityFrameInContainerSpace =
      self.regularGuide.layoutFrame;
  _remoteAccessibilityElement.accessibilityFrameInContainerSpace =
      self.remoteGuide.layoutFrame;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  // Don't recognize taps if drag touches are being tracked.
  return !self.tracking;
}

#pragma mark - Private

// Sets up all of the subviews for this control, as well as the layout guides
// used to position the section content.
- (void)setupViews {
  UIView* backgroundView = [[TabGridPageControlBackground alloc] init];
  backgroundView.userInteractionEnabled = NO;
  backgroundView.layer.cornerRadius = kCornerRadius;
  backgroundView.layer.masksToBounds = YES;
  [self addSubview:backgroundView];
  backgroundView.center =
      CGPointMake(kOverallWidth / 2.0, kOverallHeight / 2.0);

  // Set up the layout guides for the segments.
  UILayoutGuide* incognitoGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:incognitoGuide];
  self.incognitoGuide = incognitoGuide;
  UILayoutGuide* regularGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:regularGuide];
  self.regularGuide = regularGuide;
  UILayoutGuide* remoteGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:remoteGuide];
  self.remoteGuide = remoteGuide;

  // All of the guides are of the same height, and vertically centered in the
  // control.
  for (UILayoutGuide* guide in @[ incognitoGuide, regularGuide, remoteGuide ]) {
    [guide.heightAnchor constraintEqualToConstant:kOverallHeight].active = YES;
    [guide.centerYAnchor constraintEqualToAnchor:self.centerYAnchor].active =
        YES;
  }

  // Guides are all the same width (except the regular guide is wider to include
  // the separators on either side of it. The regular guide is centered in the
  // control, and the incognito and remote guides are on the leading and
  // trailing sides of it.
  [NSLayoutConstraint activateConstraints:@[
    [incognitoGuide.widthAnchor constraintEqualToConstant:kSegmentWidth],
    [regularGuide.widthAnchor
        constraintEqualToConstant:kSegmentWidth + 2 * kSeparatorWidth],
    [remoteGuide.widthAnchor constraintEqualToConstant:kSegmentWidth],
    [regularGuide.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [incognitoGuide.trailingAnchor
        constraintEqualToAnchor:regularGuide.leadingAnchor],
    [remoteGuide.leadingAnchor
        constraintEqualToAnchor:regularGuide.trailingAnchor]
  ]];

  // Add the slider above the section images and labels.
  CGRect sliderFrame = CGRectMake(0, 0, kSliderWidth, kSliderHeight);
  UIView* slider = [[UIView alloc] initWithFrame:sliderFrame];
  slider.userInteractionEnabled = NO;
  slider.layer.cornerRadius = kCornerRadius;
  slider.layer.masksToBounds = YES;
  slider.backgroundColor = UIColorFromRGB(kSliderColor);

  [self addSubview:slider];
  self.sliderView = slider;

  // Selected images and labels are added to the selected image view so they
  // will be clipped by the slider.
  UIView* selectedImageView = [[UIView alloc]
      initWithFrame:(CGRectMake(0, 0, kOverallWidth, kOverallHeight))];
  selectedImageView.userInteractionEnabled = NO;
  [self.sliderView addSubview:selectedImageView];
  self.selectedImageView = selectedImageView;

  // Icons and labels for the regular tabs.
  UIImageView* regularIcon =
      [[UIImageView alloc] initWithImage:ImageForSegment(@"regular", NO)];
  regularIcon.tintColor = UIColorFromRGB(kSliderColor);
  [self insertSubview:regularIcon belowSubview:self.sliderView];
  self.regularIcon = regularIcon;
  UIImageView* regularSelectedIcon =
      [[UIImageView alloc] initWithImage:ImageForSegment(@"regular", YES)];
  regularSelectedIcon.tintColor = UIColorFromRGB(kSelectedColor);
  [self.selectedImageView addSubview:regularSelectedIcon];
  self.regularSelectedIcon = regularSelectedIcon;
  UILabel* regularLabel = [self labelSelected:NO];
  [self insertSubview:regularLabel belowSubview:self.sliderView];
  self.regularLabel = regularLabel;
  UILabel* regularSelectedLabel = [self labelSelected:YES];
  [self.selectedImageView addSubview:regularSelectedLabel];
  self.regularSelectedLabel = regularSelectedLabel;

  // Icons for the incognito tabs section.
  UIImageView* incognitoIcon =
      [[UIImageView alloc] initWithImage:ImageForSegment(@"incognito", NO)];
  incognitoIcon.tintColor = UIColorFromRGB(kSliderColor);
  [self insertSubview:incognitoIcon belowSubview:self.sliderView];
  self.incognitoIcon = incognitoIcon;
  UIImageView* incognitoSelectedIcon =
      [[UIImageView alloc] initWithImage:ImageForSegment(@"incognito", YES)];
  incognitoSelectedIcon.tintColor = UIColorFromRGB(kSelectedColor);
  [self.selectedImageView addSubview:incognitoSelectedIcon];
  self.incognitoSelectedIcon = incognitoSelectedIcon;

  // Icons for the remote tabs section.
  UIImageView* remoteIcon =
      [[UIImageView alloc] initWithImage:ImageForSegment(@"remote", NO)];
  remoteIcon.tintColor = UIColorFromRGB(kSliderColor);
  [self insertSubview:remoteIcon belowSubview:self.sliderView];
  self.remoteIcon = remoteIcon;
  UIImageView* remoteSelectedIcon =
      [[UIImageView alloc] initWithImage:ImageForSegment(@"remote", YES)];
  remoteSelectedIcon.tintColor = UIColorFromRGB(kSelectedColor);
  [self.selectedImageView addSubview:remoteSelectedIcon];
  self.remoteSelectedIcon = remoteSelectedIcon;

  // Update the label text, in case these properties have been set before the
  // views were set up.
  self.regularTabCount = _regularTabCount;

  // Mark the control's layout as dirty so the the guides will be computed, then
  // force a layout now so it won't be triggered later (perhaps during an
  // animation).
  [self setNeedsLayout];
  [self layoutIfNeeded];

  // Add the gesture recognizer for taps on this control.
  UITapGestureRecognizer* tapRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  tapRecognizer.delegate = self;
  [self addGestureRecognizer:tapRecognizer];
}

// Creates a label for use in this control.
// Selected labels use a different size and are black.
- (UILabel*)labelSelected:(BOOL)selected {
  CGFloat size = selected ? kSelectedLabelSize : kLabelSize;
  UIColor* color =
      selected ? UIColorFromRGB(kSelectedColor) : UIColorFromRGB(kSliderColor);
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, size, size)];
  label.backgroundColor = UIColor.clearColor;
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor = color;
  label.font = [UIFont systemFontOfSize:size * .6 weight:UIFontWeightBold];
  return label;
}

// Handles tap gesture recognizer taps, setting a new selected page if the
// tap was outside the current page and sending the value changed actions.
- (void)handleTap:(UIGestureRecognizer*)tapRecognizer {
  CGPoint point = [tapRecognizer locationInView:self];
  // Determine which section the tap is in by looking at the layout frames of
  // each guide.
  TabGridPage page;
  if (CGRectContainsPoint(self.incognitoGuide.layoutFrame, point)) {
    page = TabGridPageIncognitoTabs;
  } else if (CGRectContainsPoint(self.remoteGuide.layoutFrame, point)) {
    page = TabGridPageRemoteTabs;
  } else {
    // bug: taps in the left- or rightmost |kSliderOverhang| points of the
    // control will fall through to this case.
    // TODO(crbug.com/804500): Fix this.
    page = TabGridPageRegularTabs;
  }
  if (page != self.selectedPage) {
    [self setSelectedPage:page animated:YES];
    [self sendActionsForControlEvents:UIControlEventTouchUpInside];
  }
}

// Returns the point at the center of |segment|.
- (CGPoint)centerOfSegment:(TabGridPage)segment {
  switch (segment) {
    case TabGridPageIncognitoTabs:
      return RectCenter(self.incognitoGuide.layoutFrame);
    case TabGridPageRegularTabs:
      return RectCenter(self.regularGuide.layoutFrame);
    case TabGridPageRemoteTabs:
      return RectCenter(self.remoteGuide.layoutFrame);
  }
}

@end

@implementation TabGridPageControlBackground

- (instancetype)init {
  self =
      [super initWithFrame:CGRectMake(0, 0, kBackgroundWidth, kSegmentHeight)];
  if (self) {
    self.backgroundColor = UIColor.clearColor;
  }
  return self;
}

- (CGSize)intrinsicContentsSize {
  return CGSizeMake(kBackgroundWidth, kSegmentHeight);
}

- (void)drawRect:(CGRect)rect {
  CGContextRef drawing = UIGraphicsGetCurrentContext();
  UIColor* backgroundColor = UIColorFromRGB(kBackgroundColor, kBackgroundAlpha);
  CGContextSetFillColorWithColor(drawing, backgroundColor.CGColor);
  CGRect fillRect = CGRectMake(0, 0, kSegmentWidth, kSegmentHeight);
  CGContextFillRect(drawing, fillRect);
  fillRect.origin.x += kSegmentWidth + kSeparatorWidth;
  CGContextFillRect(drawing, fillRect);
  fillRect.origin.x += kSegmentWidth + kSeparatorWidth;
  CGContextFillRect(drawing, fillRect);
}

@end
