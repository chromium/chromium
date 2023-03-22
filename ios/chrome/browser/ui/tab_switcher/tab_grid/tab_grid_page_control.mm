// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_control.h"

#import <CoreGraphics/CoreGraphics.h>
#import <algorithm>

#import "base/check_op.h"
#import "base/cxx17_backports.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIControlEvents TabGridPageChangeByTapEvent = 1 << 24;
UIControlEvents TabGridPageChangeByDragEvent = 1 << 25;

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
//  * The background view, a grey roundrect.
//  * The separators between the segment.
//  * The background image views.
//  * The numeric label on the regular tab icon.
//  * The hover views, which allow for pointer interactions.
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
const CGFloat kSliderWidth = 65.0;

// Height and width of each segment.
const CGFloat kSegmentHeight = 44.0;
const CGFloat kSegmentWidth = 65.0;

// Margin between the slider and the leading/trailing segments.
const CGFloat kSliderMargin = 2.0;

// Vertical margin between the slider and the segment on each side.
const CGFloat kSliderVerticalMargin =
    std::max((kSegmentHeight - kSliderHeight) / 2.0, 0.0);

// Width and height of the separator bars between segments.
const CGFloat kSeparatorWidth = 1.0;
const CGFloat kSeparatorHeight = 22.0;

// Overall height of the control -- the larger of the slider and segment
// heights.
const CGFloat kOverallHeight = std::max(kSliderHeight, kSegmentHeight);
// Overall width -- three segments plus two separators plus two margins between
// leading/trailing segments and the slider.
const CGFloat kOverallWidth =
    3 * kSegmentWidth + 2 * kSeparatorWidth + 2 * kSliderMargin;

// Radius used to draw the background and the slider.
const CGFloat kSliderCornerRadius = 13.0;
const CGFloat kBackgroundCornerRadius = 15.0;

// Sizes for the labels and their selected counterparts.
const CGFloat kLabelSize = 20.0;
const CGFloat kSelectedLabelSize = 23.0;
const CGFloat kLabelSizeToFontSize = 0.6;

// Maximum duration of slider motion animation.
const NSTimeInterval kSliderMoveDuration = 0.2;

// Alpha for the background view.
const CGFloat kBackgroundAlpha = 0.15;
const CGFloat kScrolledToTopBackgroundAlpha = 0.25;

// The sizes of the symbol images.
const CGFloat kUnselectedSymbolSize = 22.;
const CGFloat kSelectedSymbolSize = 24.;

// Returns the point that's at the center of `rect`.
CGPoint RectCenter(CGRect rect) {
  return CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
}

// Returns an UIImageView for the given `symbolName` and `selected` state.
UIImageView* ImageViewForSymbol(NSString* symbolName, bool selected) {
  CGFloat size = selected ? kSelectedSymbolSize : kUnselectedSymbolSize;
  return [[UIImageView alloc]
      initWithImage:CustomSymbolTemplateWithPointSize(symbolName, size)];
}

}  // namespace

@interface TabGridPageControl () <UIGestureRecognizerDelegate,
                                  UIPointerInteractionDelegate> {
  UIAccessibilityElement* _incognitoAccessibilityElement;
  UIAccessibilityElement* _regularAccessibilityElement;
  UIAccessibilityElement* _remoteAccessibilityElement;
}

// The grey background for all the segments.
@property(nonatomic, weak) UIView* background;

// Layout guides used to position segment-specific content.
@property(nonatomic, weak) UILayoutGuide* incognitoGuide;
@property(nonatomic, weak) UILayoutGuide* regularGuide;
@property(nonatomic, weak) UILayoutGuide* remoteGuide;
// The separator between incognito and regular tabs.
@property(nonatomic, weak) UIView* firstSeparator;
// The separator between the regular and remote tabs.
@property(nonatomic, weak) UIView* secondSeparator;
// The view for the slider.
@property(nonatomic, weak) UIView* sliderView;
// The view for the selected images and labels (a subview of `sliderView).
@property(nonatomic, weak) UIView* selectedImageView;
// The labels for the incognito and regular sections, in regular and selected
// variants.
@property(nonatomic, weak) UIView* incognitoNotSelectedIcon;
@property(nonatomic, weak) UIView* incognitoSelectedIcon;
@property(nonatomic, weak) UIView* regularNotSelectedIcon;
@property(nonatomic, weak) UIView* regularSelectedIcon;
@property(nonatomic, weak) UILabel* regularLabel;
@property(nonatomic, weak) UILabel* regularSelectedLabel;
@property(nonatomic, weak) UIView* remoteNotSelectedIcon;
@property(nonatomic, weak) UIView* remoteSelectedIcon;

// Standard pointer interactions provided UIKit require views on which to attach
// interactions. These transparent views are the size of the whole segment and
// are visually below the slider. All touch events are properly received by the
// parent page control. And these views properly receive hover events by a
// pointer.
@property(nonatomic, weak) UIView* incognitoHoverView;
@property(nonatomic, weak) UIView* regularHoverView;
@property(nonatomic, weak) UIView* remoteHoverView;

// The center point for the slider corresponding to a `sliderPosition` of 0.
@property(nonatomic) CGFloat sliderOrigin;
// The (signed) x-coordinate distance the slider moves over. The slider's
// position is set by adding a fraction of this distance to `sliderOrigin`, so
// that when `sliderRange` is negative (in RTL layout), the slider will move in
// the negative-x direction from `sliderOrigin`, and otherwise it will move in
// the positive-x direction.
@property(nonatomic) CGFloat sliderRange;
// State properties to track the point and position (in the 0.0-1.0 range) of
// drags.
@property(nonatomic) CGPoint dragStart;
@property(nonatomic) CGFloat dragStartPosition;
@property(nonatomic) BOOL draggingSlider;
// Gesture recognizer used to handle taps. Owned by `self` as a UIView, so this
// property is just a weak pointer to refer to it in some touch logic.
@property(nonatomic, weak) UIGestureRecognizer* tapRecognizer;

// Whether the content below is scrolled to the edge or displayed behind.
@property(nonatomic, assign) BOOL scrolledToEdge;

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

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(accessibilityBoldTextStatusDidChange)
               name:UIAccessibilityBoldTextStatusDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge {
  if (_scrolledToEdge == scrolledToEdge) {
    return;
  }

  _scrolledToEdge = scrolledToEdge;

  CGFloat backgroundAlpha =
      scrolledToEdge ? kScrolledToTopBackgroundAlpha : kBackgroundAlpha;
  self.background.backgroundColor = [UIColor colorWithWhite:1
                                                      alpha:backgroundAlpha];
}

#pragma mark - Public Properies

- (void)setSelectedPage:(TabGridPage)selectedPage {
  [self setSelectedPage:selectedPage animated:NO];
}

- (void)setSliderPosition:(CGFloat)sliderPosition {
  // Clamp `selectionOffset` to (0.0 - 1.0).
  sliderPosition = base::clamp<CGFloat>(sliderPosition, 0.0, 1.0);
  CGPoint center = self.sliderView.center;
  center.x = self.sliderOrigin + self.sliderRange * sliderPosition;
  self.sliderView.center = center;
  // Reposition the selected image view so that it's still centered in the
  // control itself.
  self.selectedImageView.center = [self convertPoint:RectCenter(self.bounds)
                                              toView:self.sliderView];
  _sliderPosition = sliderPosition;

  // `_selectedPage` should be kept in sync with the slider position.
  TabGridPage previousSelectedPage = _selectedPage;
  if (sliderPosition < 0.25)
    _selectedPage = TabGridPageIncognitoTabs;
  else if (sliderPosition < 0.75)
    _selectedPage = TabGridPageRegularTabs;
  else
    _selectedPage = TabGridPageRemoteTabs;

  // Hide/show the separator based on the slider position. Add a delta for the
  // comparison to avoid issues when the regular tabs are selected.
  const CGFloat kDelta = 0.001;
  self.firstSeparator.hidden = sliderPosition < 0.5 + kDelta;
  self.secondSeparator.hidden = sliderPosition > 0.5 - kDelta;

  if (_selectedPage != previousSelectedPage)
    [self updateSelectedPageAccessibility];
}

// Setters for the control's text values. These need to update three things:
// the text in both labels (the regular and the  "selected" versions that's
// visible when the slider is over a segment), and an ivar to store values that
// are set before the labels are created.
- (void)setRegularTabCount:(NSUInteger)regularTabCount {
  _regularTabCount = regularTabCount;
  [self updateRegularLabels];
}

- (void)setPinnedTabCount:(NSUInteger)pinnedTabCount {
  _pinnedTabCount = pinnedTabCount;
  [self updateRegularLabels];
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

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  if (self.draggingSlider) {
    return;
  }
  UITouch* touch = [touches anyObject];
  CGPoint locationInSlider = [touch locationInView:self.sliderView];
  if ([self.sliderView pointInside:locationInSlider withEvent:event]) {
    self.dragStart = [touch locationInView:self];
    self.dragStartPosition = self.sliderPosition;
    self.draggingSlider = YES;
  }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesMoved:touches withEvent:event];
  if (!self.draggingSlider)
    return;
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  UITouch* touch = [touches anyObject];
  CGPoint position = [touch locationInView:self];
  CGFloat deltaX = position.x - self.dragStart.x;
  // Convert to position change.
  CGFloat positionChange = deltaX / self.sliderRange;

  self.sliderPosition = self.dragStartPosition + positionChange;
  [self sendActionsForControlEvents:UIControlEventValueChanged];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  self.draggingSlider = NO;
  [self setSelectedPage:self.selectedPage animated:YES];
  [self sendActionsForControlEvents:TabGridPageChangeByDragEvent];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  DCHECK(!self.multipleTouchEnabled);
  DCHECK_EQ(1U, touches.count);
  self.draggingSlider = NO;
  // The tap recognizer will cancel the touches it recognizes as the last step
  // of handling the gesture, so in that case, control events have already been
  // sent and don't need to be sent again here.
  if (self.tapRecognizer.state != UIGestureRecognizerStateEnded) {
    [self setSelectedPage:self.selectedPage animated:YES];
    [self sendActionsForControlEvents:TabGridPageChangeByDragEvent];
  }
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

  // Position the section images, labels and hover views, which depend on the
  // layout guides.
  self.incognitoNotSelectedIcon.center =
      [self centerOfSegment:TabGridPageIncognitoTabs];
  self.incognitoSelectedIcon.center =
      [self centerOfSegment:TabGridPageIncognitoTabs];

  self.regularNotSelectedIcon.center =
      [self centerOfSegment:TabGridPageRegularTabs];
  self.regularSelectedIcon.center =
      [self centerOfSegment:TabGridPageRegularTabs];
  self.regularLabel.center = [self centerOfSegment:TabGridPageRegularTabs];
  self.regularSelectedLabel.center =
      [self centerOfSegment:TabGridPageRegularTabs];

  self.remoteNotSelectedIcon.center =
      [self centerOfSegment:TabGridPageRemoteTabs];
  self.remoteSelectedIcon.center = [self centerOfSegment:TabGridPageRemoteTabs];

  self.incognitoHoverView.center =
      [self centerOfSegment:TabGridPageIncognitoTabs];
  self.regularHoverView.center = [self centerOfSegment:TabGridPageRegularTabs];
  self.remoteHoverView.center = [self centerOfSegment:TabGridPageRemoteTabs];

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
  return !self.draggingSlider;
}

#pragma mark - Private

// Configures and Adds icon to the tab grid page control for the given tab.
- (void)addTabsIcon:(TabGridPage)tab {
  UIImageView* iconSelected;
  UIImageView* iconNotSelected;
  switch (tab) {
    case TabGridPageRegularTabs: {
      iconSelected = ImageViewForSymbol(kSquareNumberSymbol, true);
      iconNotSelected = ImageViewForSymbol(kSquareNumberSymbol, false);
      self.regularSelectedIcon = iconSelected;
      self.regularNotSelectedIcon = iconNotSelected;
      break;
    }
    case TabGridPageIncognitoTabs: {
      iconSelected = ImageViewForSymbol(kIncognitoSymbol, true);
      iconNotSelected = ImageViewForSymbol(kIncognitoSymbol, false);
      self.incognitoSelectedIcon = iconSelected;
      self.incognitoNotSelectedIcon = iconNotSelected;
      break;
    }
    case TabGridPageRemoteTabs: {
      iconSelected = ImageViewForSymbol(kRecentTabsSymbol, true);
      iconNotSelected = ImageViewForSymbol(kRecentTabsSymbol, false);
      self.remoteSelectedIcon = iconSelected;
      self.remoteNotSelectedIcon = iconNotSelected;
      break;
    }
  }

  iconNotSelected.tintColor = [UIColor colorNamed:kStaticGrey300Color];
  iconSelected.tintColor = UIColor.blackColor;

  [self insertSubview:iconNotSelected belowSubview:self.sliderView];
  [self.selectedImageView addSubview:iconSelected];
}

// Sets up all of the subviews for this control, as well as the layout guides
// used to position the section content.
- (void)setupViews {
  self.scrolledToEdge = YES;

  UIView* backgroundView = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, kOverallWidth, kSegmentHeight)];
  backgroundView.backgroundColor =
      [UIColor colorWithWhite:1 alpha:kScrolledToTopBackgroundAlpha];
  backgroundView.userInteractionEnabled = NO;
  backgroundView.layer.cornerRadius = kBackgroundCornerRadius;
  backgroundView.layer.masksToBounds = YES;
  [self addSubview:backgroundView];
  backgroundView.center =
      CGPointMake(kOverallWidth / 2.0, kOverallHeight / 2.0);
  self.background = backgroundView;

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
    // Guides are all the same width. The regular guide is centered in the
    // control, and the incognito and remote guides are on the leading and
    // trailing sides of it, with separators in between.
    [guide.widthAnchor constraintEqualToConstant:kSegmentWidth].active = YES;
    [guide.centerYAnchor constraintEqualToAnchor:self.centerYAnchor].active =
        YES;
  }

  UIView* firstSeparator = [self newSeparator];
  [self addSubview:firstSeparator];
  self.firstSeparator = firstSeparator;
  UIView* secondSeparator = [self newSeparator];
  [self addSubview:secondSeparator];
  self.secondSeparator = secondSeparator;

  [NSLayoutConstraint activateConstraints:@[
    [incognitoGuide.trailingAnchor
        constraintEqualToAnchor:firstSeparator.leadingAnchor],
    [firstSeparator.trailingAnchor
        constraintEqualToAnchor:regularGuide.leadingAnchor],
    [regularGuide.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [regularGuide.trailingAnchor
        constraintEqualToAnchor:secondSeparator.leadingAnchor],
    [secondSeparator.trailingAnchor
        constraintEqualToAnchor:remoteGuide.leadingAnchor],

    [firstSeparator.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [secondSeparator.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
  ]];

  // Add the slider above the section images and labels.
  CGRect sliderFrame =
      CGRectMake(0, kSliderVerticalMargin, kSliderWidth, kSliderHeight);
  UIView* slider = [[UIView alloc] initWithFrame:sliderFrame];
  slider.layer.cornerRadius = kSliderCornerRadius;
  slider.layer.masksToBounds = YES;
  slider.backgroundColor = UIColor.whiteColor;

  [self addSubview:slider];
  self.sliderView = slider;

  // Selected images and labels are added to the selected image view so they
  // will be clipped by the slider.
  CGRect selectedImageFrame = CGRectMake(0, 0, kOverallWidth, kOverallHeight);
  UIView* selectedImageView = [[UIView alloc] initWithFrame:selectedImageFrame];
  selectedImageView.userInteractionEnabled = NO;
  [self.sliderView addSubview:selectedImageView];
  self.selectedImageView = selectedImageView;

  [self addTabsIcon:TabGridPageRegularTabs];
  [self addTabsIcon:TabGridPageIncognitoTabs];
  [self addTabsIcon:TabGridPageRemoteTabs];

  UILabel* regularLabel = [self labelSelected:NO];
  [self insertSubview:regularLabel belowSubview:self.sliderView];
  self.regularLabel = regularLabel;
  UILabel* regularSelectedLabel = [self labelSelected:YES];
  [self.selectedImageView addSubview:regularSelectedLabel];
  self.regularSelectedLabel = regularSelectedLabel;

  CGRect segmentRect = CGRectMake(0, 0, kSegmentWidth, kSegmentHeight);
  UIView* incognitoHoverView = [[UIView alloc] initWithFrame:segmentRect];
  UIView* regularHoverView = [[UIView alloc] initWithFrame:segmentRect];
  UIView* remoteHoverView = [[UIView alloc] initWithFrame:segmentRect];
  [self insertSubview:incognitoHoverView belowSubview:self.sliderView];
  [self insertSubview:regularHoverView belowSubview:self.sliderView];
  [self insertSubview:remoteHoverView belowSubview:self.sliderView];
  self.incognitoHoverView = incognitoHoverView;
  self.regularHoverView = regularHoverView;
  self.remoteHoverView = remoteHoverView;

  [self.incognitoHoverView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
  [self.regularHoverView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
  [self.remoteHoverView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];
  [self.sliderView
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

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
  self.tapRecognizer = tapRecognizer;
}

// Updates the labels displaying the regular tab count.
- (void)updateRegularLabels {
  NSUInteger totalTabsCount = self.regularTabCount + self.pinnedTabCount;
  self.regularLabel.attributedText =
      TextForTabCount(totalTabsCount, kLabelSize * kLabelSizeToFontSize);
  self.regularSelectedLabel.attributedText = TextForTabCount(
      totalTabsCount, kSelectedLabelSize * kLabelSizeToFontSize);
}

// Creates a label for use in this control.
// Selected labels use a different size and are black.
- (UILabel*)labelSelected:(BOOL)selected {
  CGFloat size = selected ? kSelectedLabelSize : kLabelSize;
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, size, size)];
  label.backgroundColor = UIColor.clearColor;
  label.textAlignment = NSTextAlignmentCenter;
  label.textColor =
      selected ? UIColor.blackColor : [UIColor colorNamed:kStaticGrey300Color];
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
    // bug: taps in the left- or rightmost `kSliderOverhang` points of the
    // control will fall through to this case.
    // TODO(crbug.com/804500): Fix this.
    page = TabGridPageRegularTabs;
  }
  if (page != self.selectedPage) {
    [self setSelectedPage:page animated:YES];
    [self sendActionsForControlEvents:TabGridPageChangeByTapEvent];
  }
}

// Returns the point at the center of `segment`.
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

// Creates and returns a new separator, with constraints on its height/width.
- (UIView*)newSeparator {
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kStaticGrey300Color];
  separator.layer.cornerRadius = kSeparatorWidth / 2.0;
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight].active =
      YES;
  [separator.widthAnchor constraintEqualToConstant:kSeparatorWidth].active =
      YES;
  return separator;
}

// Callback for the notification that the user changed the bold status.
- (void)accessibilityBoldTextStatusDidChange {
  [self updateRegularLabels];
}

#pragma mark UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  UIPointerHighlightEffect* effect = [UIPointerHighlightEffect
      effectWithPreview:[[UITargetedPreview alloc]
                            initWithView:interaction.view]];
  UIPointerShape* shape =
      [UIPointerShape shapeWithRoundedRect:interaction.view.frame
                              cornerRadius:kSliderCornerRadius];
  return [UIPointerStyle styleWithEffect:effect shape:shape];
}

@end
