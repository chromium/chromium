// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"

#include <cmath>
#include <memory>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#import "ios/chrome/browser/drag_and_drop/drop_and_navigate_delegate.h"
#import "ios/chrome/browser/drag_and_drop/drop_and_navigate_interaction.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#include "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#include "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_long_press_delegate.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_constants.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_view.h"
#import "ios/chrome/browser/ui/tabs/tab_view.h"
#include "ios/chrome/browser/ui/tabs/target_frame_cache.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_favicon_driver_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

// Animation duration for tab animations.
const NSTimeInterval kTabAnimationDuration = 0.25;

// Animation duration for tab strip fade.
const NSTimeInterval kTabStripFadeAnimationDuration = 0.15;

// Amount of time needed to trigger drag and drop mode when long pressing.
const NSTimeInterval kDragAndDropLongPressDuration = 0.4;

// Tab dimensions.
const CGFloat kTabOverlapStacked = 32.0;
const CGFloat kTabOverlapUnstacked = 30.0;

const CGFloat kNewTabOverlap = 13.0;
const CGFloat kMaxTabWidthStacked = 265.0;
const CGFloat kMaxTabWidthUnstacked = 225.0;

// Tab Switcher button dimensions.
const CGFloat kTabSwitcherButtonWidth = 46.0;
const CGFloat kTabSwitcherButtonBackgroundWidth = 62.0;

const CGFloat kMinTabWidthStacked = 200.0;
const CGFloat kMinTabWidthUnstacked = 160.0;

const CGFloat kCollapsedTabOverlap = 5.0;
const NSUInteger kMaxNumCollapsedTabsStacked = 3;
const NSUInteger kMaxNumCollapsedTabsUnstacked = 0;

// Tabs with a visible width smaller than this draw as collapsed tabs..
const CGFloat kCollapsedTabWidthThreshold = 40.0;

// Autoscroll constants.  The autoscroll distance is set to
// |kMaxAutoscrollDistance| at the edges of the scroll view.  As the tab moves
// away from the edges of the scroll view, the autoscroll distance decreases by
// one for each |kAutoscrollDecrementWidth| points.
const CGFloat kMaxAutoscrollDistance = 10.0;
const CGFloat kAutoscrollDecrementWidth = 10.0;

// Dimming view constants, in points.
const CGFloat kDimmingViewBottomInsetHighRes = 0.0;
const CGFloat kDimmingViewBottomInset = 0.0;

// The size of the tab strip view.
const CGFloat kTabStripHeight = 39.0;

// The size of the new tab button.
const CGFloat kNewTabButtonWidth = 59.9;

// Default image insets for the new tab button.
const CGFloat kNewTabButtonHorizontalImageInset = 2.0;
const CGFloat kNewTabButtonTopImageInset = 6.0;
const CGFloat kNewTabButtonBottomImageInset = 7.0;

// Offsets needed to keep the UI properly centered on high-res screens, in
// points.
const CGFloat kNewTabButtonBottomOffsetHighRes = 2.0;

// Returns the background color.
UIColor* BackgroundColor() {
  return UIColor.blackColor;
}

}  // namespace

// Helper class to display a UIButton with the image and text centered
// vertically and horizontally.
@interface TabStripCenteredButton : UIButton {
}
@end

@implementation TabStripCenteredButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    self.titleLabel.textAlignment = NSTextAlignmentCenter;
    self.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightBold];
    self.titleLabel.adjustsFontSizeToFitWidth = YES;
    self.titleLabel.minimumScaleFactor = 0.1;
    self.titleLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGSize size = self.bounds.size;
  CGPoint center = CGPointMake(size.width / 2, size.height / 2);
  self.imageView.center = center;
  self.imageView.frame = AlignRectToPixel(self.imageView.frame);
  self.titleLabel.frame = self.bounds;
}

@end

@interface TabStripController () <DropAndNavigateDelegate,
                                  CRWWebStateObserver,
                                  TabStripViewLayoutDelegate,
                                  TabViewDelegate,
                                  WebStateListObserving,
                                  WebStateFaviconDriverObserver,
                                  UIGestureRecognizerDelegate,
                                  UIScrollViewDelegate> {
  TabModel* _tabModel;
  UIView* _view;
  TabStripView* _tabStripView;
  UIButton* _buttonNewTab;
  UIButton* _tabSwitcherButton;

  // Background view of the tab switcher button. Only visible while in unstacked
  // layout.
  UIImageView* _tabSwitcherButtonBackgroundView;

  TabStripStyle _style;

  // Array of TabViews.  There is a one-to-one correspondence between this array
  // and the set of Tabs in the TabModel.
  NSMutableArray* _tabArray;

  // Set of TabViews that are currently closing.  These TabViews are also in
  // |_tabArray|.  Used to translate between |_tabArray| indexes and TabModel
  // indexes.
  NSMutableSet* _closingTabs;

  // Tracks target frames for TabViews.
  // TODO(rohitrao): This is unnecessary, as UIKit updates view frames
  // immediately, so [view frame] will always return the end state of the
  // current animation.  We can remove this cache entirely.  b/5516053
  TargetFrameCache _targetFrames;

  // Animate when doing layout.  This flag is set by setNeedsLayoutWithAnimation
  // and cleared in layoutSubviews.
  BOOL _animateLayout;

  // The current tab width.  Recomputed whenever a tab is added or removed.
  CGFloat _currentTabWidth;

  // View used to dim unselected tabs when in reordering mode.  Nil when not
  // reordering tabs.
  UIView* _dimmingView;

  // Is the selected tab highlighted, used when dragging or swiping tabs.
  BOOL _highlightsSelectedTab;

  // YES when in reordering mode.
  // TODO(rohitrao): This is redundant with |_draggedTab|.  Remove it.
  BOOL _isReordering;

  // The tab that is currently being dragged.  nil when not in reordering mode.
  TabView* _draggedTab;

  // The last known location of the touch that is dragging the tab.  This
  // location is in the coordinate system of |[_tabStripView superview]| because
  // that coordinate system does not change as the scroll view scrolls.
  CGPoint _lastDragLocation;

  // Timer used to autoscroll when in reordering mode.  Is nil when not active.
  // Owned by its runloop.
  __weak NSTimer* _autoscrollTimer;  // weak

  // The distance to scroll for each autoscroll timer tick.  If negative, the
  // tabstrip will scroll to the left; if positive, to the right.
  CGFloat _autoscrollDistance;

  // The model index of the placeholder gap, if one exists.  This value is used
  // as the new model index of the dragged tab when it is dropped.
  NSUInteger _placeholderGapModelIndex;

  // YES if this tab strip is representing an incognito TabModel.
  BOOL _isIncognito;

  // The disabler that prevents the toolbar from being scrolled offscreen during
  // drags.
  std::unique_ptr<ScopedFullscreenDisabler> _fullscreenDisabler;

  // Bridges C++ WebStateListObserver methods to this TabStripController.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Bridges FaviconDriverObservers methods to this TabStripController, and
  // maintains a FaviconObserver for each all webstates.
  std::unique_ptr<WebStateListFaviconDriverObserver>
      _webStateListFaviconObserver;

  // Bridges C++ WebStateObserver methods to this TabStripController.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;

  // Forwards observer methods for all WebStates in the WebStateList monitored
  // by the TabStripController.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;

  API_AVAILABLE(ios(11.0)) DropAndNavigateInteraction* _buttonNewTabInteraction;
}

@property(nonatomic, readonly, retain) TabStripView* tabStripView;
@property(nonatomic, readonly, retain) UIButton* buttonNewTab;

// YES if the controller has been disconnected.
@property(nonatomic) BOOL disconnected;

// If set to |YES|, tabs at either end of the tabstrip are "collapsed" into a
// stack, such that the visible width of the tabstrip is constant.  If set to
// |NO|, tabs are never collapsed and the tabstrip scrolls horizontally as a
// normal scroll view would.  Changing this property causes the tabstrip to
// redraw and relayout.  Defaults to |YES|.
@property(nonatomic, assign) BOOL useTabStacking;

// Initializes the tab array based on the the entries in the TabModel.  Creates
// one TabView per Tab and adds it to the tabstrip.  A later call to
// |-layoutTabs| is needed to properly place the tabs in the correct positions.
- (void)initializeTabArrayFromTabModel;

// Initializes the tab array to have only one empty tab, for the case (used
// during startup) when there is not a tab model available.
- (void)initializeTabArrayWithNoModel;

// Returns an autoreleased TabView object with no content.
- (TabView*)emptyTabView;

// Returns an autoreleased TabView object based on the given |webState|.
// |isSelected| is passed in here as an optimization, so that the TabView is
// drawn correctly the first time, without requiring the model to send a
// -setSelected message to the TabView.
- (TabView*)createTabViewForWebState:(web::WebState*)webState
                          isSelected:(BOOL)isSelected;

// Creates and installs the view used to dim unselected tabs.  Does nothing if
// the view already exists.
- (void)installDimmingViewWithAnimation:(BOOL)animate;

// Remove the dimming view,
- (void)removeDimmingViewWithAnimation:(BOOL)animate;

// Converts between model indexes and |_tabArray| indexes.  The conversion is
// necessary because |_tabArray| contains closing tabs whereas the TabModel does
// not.
- (NSUInteger)indexForModelIndex:(NSUInteger)modelIndex;
- (NSUInteger)modelIndexForIndex:(NSUInteger)index;
- (NSUInteger)modelIndexForTabView:(TabView*)view;

// Helper methods to handle each stage of a drag.
- (void)beginDrag:(UILongPressGestureRecognizer*)gesture;
- (void)continueDrag:(UILongPressGestureRecognizer*)gesture;
- (void)endDrag:(UILongPressGestureRecognizer*)gesture;
- (void)cancelDrag:(UILongPressGestureRecognizer*)gesture;

// Resets any internal variables used to track drag state.
- (void)resetDragState;

// Returns whether or not the tabstrip is currently in reordering mode.
- (BOOL)isReorderingTabs;

// Installs or removes the autoscroll timer.
- (void)installAutoscrollTimerIfNeeded;
- (void)removeAutoscrollTimer;

// Called once per autoscroll timer tick.  Adjusts the scroll view's content
// offset as needed.
- (void)autoscrollTimerFired:(NSTimer*)timer;

// Calculates and stores the autoscroll distance for the given tab view.  The
// autoscroll distance is a function of the distance between the edge of the
// scroll view and the tab's frame.
- (void)computeAutoscrollDistanceForTabView:(TabView*)view;

// Constrains the stored autoscroll distance to prevent the scroll view from
// overscrolling.
- (void)constrainAutoscrollDistance;

#if 0
// Returns the appropriate model index for the currently dragged tab, given its
// current position.  (If dropped, the tab would be at this index in the model.)
// TODO(rohitrao): Implement this method.
- (NSUInteger)modelIndexForDraggedTab;
#endif

// Returns the horizontal visible tab strip width used to compute the tab width
// and the tabs and new tab button in regular layout mode.
- (CGFloat)tabStripVisibleSpace;

// Shift all of the tab strip subviews by an amount equal to the content offset
// change, which effectively places the subviews back where they were before the
// change, in terms of screen coordinates.
- (void)shiftTabStripSubviews:(CGPoint)oldContentOffset;

// Updates the scroll view's content size based on the current set of tabs and
// closing tabs.  After updating the content size, repositions views so they
// they will appear stationary on screen.
- (void)updateContentSizeAndRepositionViews;

// Returns the frame, in the scroll view content's coordinate system, of the
// given tab view.
- (CGRect)scrollViewFrameForTab:(TabView*)view;

// Returns the portion of |frame| which is not covered by |frameOnTop|.
- (CGRect)calculateVisibleFrameForFrame:(CGRect)frame
                         whenUnderFrame:(CGRect)frameOnTop;

// Schedules a layout of the scroll view and sets the internal |_animateLayout|
// flag so that the layout will be animated.
- (void)setNeedsLayoutWithAnimation;

// Returns the maximum number of collapsed tabs depending on the current layout
// mode.
- (NSUInteger)maxNumCollapsedTabs;

// Returns the tab overlap width depending on the current layout mode.
- (CGFloat)tabOverlap;

// Returns the maximum tab view width depending on the current layout mode.
- (CGFloat)maxTabWidth;

// Returns the minimum tab view width depending on the current layout mode.
- (CGFloat)minTabWidth;

// Automatically scroll the tab strip view to keep the given tab view visible.
// This method must be called with a valid |tabIndex|.
- (void)scrollTabToVisible:(NSUInteger)tabIndex;

// Updates the content offset of the tab strip view in order to keep the
// selected tab view visible.
// Content offset adjustement is only needed/performed in unstacked mode or
// regular mode for newly opened webStates.
// This method must be called with a valid |WebStateIndex|.
- (void)updateContentOffsetForWebStateIndex:(int)WebStateIndex
                              isNewWebState:(BOOL)isNewWebState;

// Update the frame of the tab strip view (scrollview) frame, content inset and
// toggle buttons states depending on the current layout mode.
- (void)updateScrollViewFrameForTabSwitcherButton;

// Updates the tab switcher button with the current tab count.
- (void)updateTabCount;

// Returns the existing tab view for |webState| or nil if there is no TabView
// for it.
- (TabView*)tabViewForWebState:(web::WebState*)webState;

// Computes whether the tabstrip should use tab stacking.
- (BOOL)shouldUseTabStacking;

@end

@implementation TabStripController

@synthesize buttonNewTab = _buttonNewTab;
@synthesize highlightsSelectedTab = _highlightsSelectedTab;
@synthesize tabStripView = _tabStripView;
@synthesize view = _view;
@synthesize dispatcher = _dispatcher;
@synthesize longPressDelegate = _longPressDelegate;
@synthesize presentationProvider = _presentationProvider;
@synthesize animationWaitDuration = _animationWaitDuration;

- (instancetype)initWithTabModel:(TabModel*)tabModel
                           style:(TabStripStyle)style
                      dispatcher:
                          (id<ApplicationCommands, BrowserCommands>)dispatcher {
  if ((self = [super init])) {
    _tabArray = [[NSMutableArray alloc] initWithCapacity:10];
    _closingTabs = [[NSMutableSet alloc] initWithCapacity:5];

    _tabModel = tabModel;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _tabModel.webStateList->AddObserver(_webStateListObserver.get());
    _webStateListFaviconObserver =
        std::make_unique<WebStateListFaviconDriverObserver>(
            _tabModel.webStateList, self);
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    // Observe all webStates of this |_tabModel.webStateList|.
    _allWebStateObservationForwarder =
        std::make_unique<AllWebStateObservationForwarder>(
            _tabModel.webStateList, _webStateObserver.get());
    _style = style;
    _dispatcher = dispatcher;

    // |self.view| setup.
    _useTabStacking = [self shouldUseTabStacking];
    CGRect tabStripFrame = [UIApplication sharedApplication].keyWindow.bounds;
    tabStripFrame.size.height = kTabStripHeight;
    _view = [[UIView alloc] initWithFrame:tabStripFrame];
    _view.autoresizingMask = (UIViewAutoresizingFlexibleWidth |
                              UIViewAutoresizingFlexibleBottomMargin);
    _view.backgroundColor = BackgroundColor();
    if (UseRTLLayout())
      _view.transform = CGAffineTransformMakeScale(-1, 1);

    // |self.tabStripView| setup.
    _tabStripView = [[TabStripView alloc] initWithFrame:_view.bounds];
    _tabStripView.autoresizingMask =
        (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
    _tabStripView.backgroundColor = _view.backgroundColor;
    _tabStripView.layoutDelegate = self;
    _tabStripView.accessibilityIdentifier =
        style == INCOGNITO ? @"Incognito Tab Strip" : @"Tab Strip";
    [_view addSubview:_tabStripView];

    // |self.buttonNewTab| setup.
    CGRect buttonNewTabFrame = tabStripFrame;
    buttonNewTabFrame.size.width = kNewTabButtonWidth;
    _buttonNewTab = [[UIButton alloc] initWithFrame:buttonNewTabFrame];
    _isIncognito = tabModel && tabModel.browserState->IsOffTheRecord();
    // TODO(crbug.com/600829): Rewrite layout code and convert these masks to
    // to trailing and leading margins rather than right and bottom.
    _buttonNewTab.autoresizingMask = (UIViewAutoresizingFlexibleRightMargin |
                                      UIViewAutoresizingFlexibleBottomMargin);
    _buttonNewTab.imageView.contentMode = UIViewContentModeCenter;

    UIImage* buttonNewTabImage = [UIImage imageNamed:@"tabstrip_new_tab"];
    [_buttonNewTab setImage:buttonNewTabImage forState:UIControlStateNormal];

    UIImage* buttonNewTabPressedImage =
        [UIImage imageNamed:@"tabstrip_new_tab_pressed"];
    [_buttonNewTab setImage:buttonNewTabPressedImage
                   forState:UIControlStateHighlighted];

    UIEdgeInsets imageInsets = UIEdgeInsetsMake(
        kNewTabButtonTopImageInset, kNewTabButtonHorizontalImageInset,
        kNewTabButtonBottomImageInset, kNewTabButtonHorizontalImageInset);
    if (IsHighResScreen()) {
      imageInsets.top += kNewTabButtonBottomOffsetHighRes;
      imageInsets.bottom -= kNewTabButtonBottomOffsetHighRes;
    }
    _buttonNewTab.imageEdgeInsets = imageInsets;
    SetA11yLabelAndUiAutomationName(
        _buttonNewTab,
        _isIncognito ? IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB
                     : IDS_IOS_TOOLS_MENU_NEW_TAB,
        _isIncognito ? @"New Incognito Tab" : @"New Tab");
    [_buttonNewTab addTarget:self
                      action:@selector(sendNewTabCommand)
            forControlEvents:UIControlEventTouchUpInside];
    [_buttonNewTab addTarget:self
                      action:@selector(recordUserMetrics:)
            forControlEvents:UIControlEventTouchUpInside];

    if (DragAndDropIsEnabled()) {
      _buttonNewTabInteraction =
          [[DropAndNavigateInteraction alloc] initWithDelegate:self];
      [_buttonNewTab addInteraction:_buttonNewTabInteraction];
    }

    [_tabStripView addSubview:_buttonNewTab];

    [self installTabSwitcherButton];

    // Add tab buttons to tab strip.
    if (_tabModel)
      [self initializeTabArrayFromTabModel];
    else
      [self initializeTabArrayWithNoModel];

    // Update the layout of the tab buttons.
    [self updateContentSizeAndRepositionViews];
    [self layoutTabStripSubviews];

    // Don't highlight the selected tab by default.
    self.highlightsSelectedTab = NO;

    // Register for VoiceOver notifications.
    if (base::FeatureList::IsEnabled(kVoiceOverUnstackedTabstrip)) {
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(voiceOverStatusDidChange)
                 name:UIAccessibilityVoiceOverStatusDidChangeNotification
               object:nil];
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(_disconnected);
}

- (void)disconnect {
  [_tabStripView setDelegate:nil];
  [_tabStripView setLayoutDelegate:nil];
  _allWebStateObservationForwarder.reset();
  _webStateListFaviconObserver.reset();
  _tabModel.webStateList->RemoveObserver(_webStateListObserver.get());
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  self.disconnected = YES;
}

- (void)hideTabStrip:(BOOL)hidden {
  self.view.hidden = hidden;
  [self updateTabSwitcherGuide];
}

#pragma mark - Private

- (void)initializeTabArrayFromTabModel {
  DCHECK(_tabModel);
  WebStateList* webStateList = _tabModel.webStateList;
  for (int index = 0; index < webStateList->count(); ++index) {
    web::WebState* webState = webStateList->GetWebStateAt(index);
    BOOL isSelected = index == webStateList->active_index();
    TabView* view = [self createTabViewForWebState:webState
                                        isSelected:isSelected];
    [_tabArray addObject:view];
    [_tabStripView addSubview:view];
  }
}

- (void)initializeTabArrayWithNoModel {
  DCHECK(!_tabModel);
  TabView* view = [self emptyTabView];
  [_tabArray addObject:view];
  [_tabStripView addSubview:view];
  [view setSelected:YES];
  return;
}

- (TabView*)emptyTabView {
  TabView* view = [[TabView alloc] initWithEmptyView:YES selected:YES];
  [view setIncognitoStyle:(_style == INCOGNITO)];
  [view setContentMode:UIViewContentModeRedraw];

  // Setting the tab to be hidden marks it as a new tab.  The layout code will
  // make the tab visible and set up the appropriate animations.
  [view setHidden:YES];

  return view;
}

- (TabView*)createTabViewForWebState:(web::WebState*)webState
                          isSelected:(BOOL)isSelected {
  TabView* view = [[TabView alloc] initWithEmptyView:NO selected:isSelected];
  if (UseRTLLayout())
    [view setTransform:CGAffineTransformMakeScale(-1, 1)];
  [view setIncognitoStyle:(_style == INCOGNITO)];
  [view setContentMode:UIViewContentModeRedraw];
  [self updateTabView:view withWebState:webState];
  // Install a long press gesture recognizer to handle drag and drop.
  UILongPressGestureRecognizer* longPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [longPress setMinimumPressDuration:kDragAndDropLongPressDuration];
  [longPress setDelegate:self];
  [view addGestureRecognizer:longPress];

  // Giving the tab view exclusive touch prevents other views from receiving
  // touches while a TabView is handling a touch.
  [view setExclusiveTouch:YES];

  // Setting the tab to be hidden marks it as a new tab.  The layout code will
  // make the tab visible and set up the appropriate animations.
  [view setHidden:YES];

  view.delegate = self;

  return view;
}

- (void)setHighlightsSelectedTab:(BOOL)highlightsSelectedTab {
  if (highlightsSelectedTab)
    [self installDimmingViewWithAnimation:YES];
  else
    [self removeDimmingViewWithAnimation:YES];

  _highlightsSelectedTab = highlightsSelectedTab;
}

- (void)installDimmingViewWithAnimation:(BOOL)animate {
  // The dimming view should not cover the bottom 2px of the tab strip, as those
  // pixels are visually part of the top border of the toolbar.  The bottom
  // inset constants take into account the conversion from pixels to points.
  CGRect frame = [_tabStripView bounds];
  frame.size.height -= (IsHighResScreen() ? kDimmingViewBottomInsetHighRes
                                          : kDimmingViewBottomInset);

  // Create the dimming view if it doesn't exist.  In all cases, make sure it's
  // set up correctly.
  if (_dimmingView)
    [_dimmingView setFrame:frame];
  else
    _dimmingView = [[UIView alloc] initWithFrame:frame];

  // Enable user interaction in order to eat touches from views behind it.
  [_dimmingView setUserInteractionEnabled:YES];
  [_dimmingView
      setBackgroundColor:[BackgroundColor() colorWithAlphaComponent:0]];
  [_dimmingView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                     UIViewAutoresizingFlexibleHeight)];
  [_tabStripView addSubview:_dimmingView];

  CGFloat duration = animate ? kTabStripFadeAnimationDuration : 0;
  [UIView animateWithDuration:duration
                   animations:^{
                     [_dimmingView
                         setBackgroundColor:[BackgroundColor()
                                                colorWithAlphaComponent:0.6]];
                   }];
}

- (void)removeDimmingViewWithAnimation:(BOOL)animate {
  if (_dimmingView) {
    CGFloat duration = animate ? kTabStripFadeAnimationDuration : 0;
    [UIView animateWithDuration:duration
        animations:^{
          [_dimmingView
              setBackgroundColor:[BackgroundColor() colorWithAlphaComponent:0]];
        }
        completion:^(BOOL finished) {
          // Do not remove the dimming view if the animation was aborted.
          if (finished) {
            [_dimmingView removeFromSuperview];
            _dimmingView = nil;
          }
        }];
  }
}

- (void)recordUserMetrics:(id)sender {
  if (sender == _buttonNewTab)
    base::RecordAction(UserMetricsAction("MobileTabStripNewTab"));
  else if (sender == _tabSwitcherButton)
    base::RecordAction(UserMetricsAction("MobileTabSwitcherOpen"));
  else
    LOG(WARNING) << "Trying to record metrics for unknown sender "
                 << base::SysNSStringToUTF8([sender description]);
}

- (void)sendNewTabCommand {
  CGPoint center = [_buttonNewTab.superview convertPoint:_buttonNewTab.center
                                                  toView:_buttonNewTab.window];
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithIncognito:_isIncognito originPoint:center];
  [self.dispatcher openURLInNewTab:command];
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gesture {
  switch ([gesture state]) {
    case UIGestureRecognizerStateBegan:
      [[NSNotificationCenter defaultCenter]
          postNotificationName:kTabStripDragStarted
                        object:nil];
      [self beginDrag:gesture];
      break;
    case UIGestureRecognizerStateChanged:
      [self continueDrag:gesture];
      break;
    case UIGestureRecognizerStateEnded:
      [self endDrag:gesture];
      [[NSNotificationCenter defaultCenter]
          postNotificationName:kTabStripDragEnded
                        object:nil];
      break;
    case UIGestureRecognizerStateCancelled:
      [self cancelDrag:gesture];
      [[NSNotificationCenter defaultCenter]
          postNotificationName:kTabStripDragEnded
                        object:nil];
      break;
    default:
      NOTREACHED();
  }
}

- (NSUInteger)indexForModelIndex:(NSUInteger)modelIndex {
  NSUInteger index = modelIndex;
  NSUInteger i = 0;
  for (TabView* tab in _tabArray) {
    if ([_closingTabs containsObject:tab])
      ++index;

    if (i == index)
      break;

    ++i;
  }
  DCHECK_GE(index, modelIndex);
  return index;
}

- (NSUInteger)modelIndexForIndex:(NSUInteger)index {
  NSUInteger modelIndex = 0;
  NSUInteger arrayIndex = 0;
  for (TabView* tab in _tabArray) {
    if (arrayIndex == index) {
      if ([_closingTabs containsObject:tab])
        return NSNotFound;
      return modelIndex;
    }

    if (![_closingTabs containsObject:tab])
      ++modelIndex;

    ++arrayIndex;
  }

  return NSNotFound;
}

- (NSUInteger)modelIndexForTabView:(TabView*)view {
  return [self modelIndexForIndex:[_tabArray indexOfObject:view]];
}

// The |tabSwitcherGuide| cannot use constrainedView in the tab strip because
// here views use CGAffineTransformMakeScale to support RTL, and NamedGuide
// doesn't honor transforms. Instead we set the tabSwitcherGuide as necessary.
- (void)updateTabSwitcherGuide {
  NamedGuide* tabSwitcherGuide =
      [NamedGuide guideWithName:kTabStripTabSwitcherGuide view:self.view];
  if (self.view.hidden) {
    tabSwitcherGuide.constrainedFrame = CGRectZero;
  } else {
    tabSwitcherGuide.constrainedFrame =
        [_tabSwitcherButton.superview convertRect:_tabSwitcherButton.frame
                                           toView:tabSwitcherGuide.owningView];
  }
}

// Updates the title and the favicon of the |view| with data from |webState|.
- (void)updateTabView:(TabView*)view withWebState:(web::WebState*)webState {
  [[view titleLabel] setText:tab_util::GetTabTitle(webState)];
  [view setFavicon:nil];
  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState);
  if (faviconDriver && faviconDriver->FaviconIsValid()) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty())
      [view setFavicon:favicon.ToUIImage()];
  }
  [_tabStripView setNeedsLayout];
}

#pragma mark -
#pragma mark Tab Drag and Drop methods

- (void)beginDrag:(UILongPressGestureRecognizer*)gesture {
  DCHECK([[gesture view] isKindOfClass:[TabView class]]);
  TabView* view = (TabView*)[gesture view];

  // Sanity checks.
  NSUInteger index = [self modelIndexForTabView:view];
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(index));
  if (index == NSNotFound)
    return;

  // Install the dimming view, hide the new tab button, and select the tab so it
  // appears highlighted.
  self.highlightsSelectedTab = YES;
  _buttonNewTab.hidden = YES;
  _tabModel.webStateList->ActivateWebStateAt(index);

  // Set up initial drag state.
  _lastDragLocation = [gesture locationInView:[_tabStripView superview]];
  _isReordering = YES;
  _draggedTab = view;
  _placeholderGapModelIndex = [self modelIndexForTabView:_draggedTab];

  // Update the autoscroll distance and timer.
  [self computeAutoscrollDistanceForTabView:_draggedTab];
  if (_autoscrollDistance != 0)
    [self installAutoscrollTimerIfNeeded];
  else
    [self removeAutoscrollTimer];

  // Disable fullscreen during drags.
  _fullscreenDisabler = std::make_unique<ScopedFullscreenDisabler>(
      FullscreenControllerFactory::GetInstance()->GetForBrowserState(
          _tabModel.browserState));
}

- (void)continueDrag:(UILongPressGestureRecognizer*)gesture {
  DCHECK([[gesture view] isKindOfClass:[TabView class]]);
  TabView* view = (TabView*)[gesture view];

  // Update the position of the dragged tab.
  CGPoint location = [gesture locationInView:[_tabStripView superview]];
  CGFloat dx = location.x - _lastDragLocation.x;
  CGRect frame = [view frame];
  frame.origin.x += dx;
  [view setFrame:frame];
  _lastDragLocation = location;

  // Update the autoscroll distance and timer.
  [self computeAutoscrollDistanceForTabView:_draggedTab];
  if (_autoscrollDistance != 0)
    [self installAutoscrollTimerIfNeeded];
  else
    [self removeAutoscrollTimer];

  [self setNeedsLayoutWithAnimation];
}

- (void)endDrag:(UILongPressGestureRecognizer*)gesture {
  DCHECK([[gesture view] isKindOfClass:[TabView class]]);

  // Stop disabling fullscreen.
  _fullscreenDisabler = nullptr;

  NSUInteger fromIndex = [self modelIndexForTabView:_draggedTab];
  // TODO(rohitrao): We're seeing crashes where fromIndex is NSNotFound,
  // indicating that the dragged tab is no longer in the TabModel.  This could
  // happen if a tab closed itself during a drag.  Investigate this further, but
  // for now, simply test |fromIndex| before proceeding.
  if (fromIndex == NSNotFound) {
    [self resetDragState];
    [self setNeedsLayoutWithAnimation];
    return;
  }

  NSUInteger toIndex = _placeholderGapModelIndex;
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(toIndex));
  DCHECK_LT(toIndex, [_tabModel count]);

  // Reset drag state variables before notifying the model that the tab moved.
  [self resetDragState];
  _tabModel.webStateList->MoveWebStateAt(static_cast<int>(fromIndex),
                                         static_cast<int>(toIndex));
  [self setNeedsLayoutWithAnimation];
}

- (void)cancelDrag:(UILongPressGestureRecognizer*)gesture {
  DCHECK([[gesture view] isKindOfClass:[TabView class]]);

  // Stop disabling fullscreen.
  _fullscreenDisabler = nullptr;

  // Reset drag state and trigger a relayout to moved tabs back into their
  // correct positions.
  [self resetDragState];
  [self setNeedsLayoutWithAnimation];
}

- (void)resetDragState {
  self.highlightsSelectedTab = NO;
  _buttonNewTab.hidden = NO;
  [self removeAutoscrollTimer];

  _isReordering = NO;
  _placeholderGapModelIndex = NSNotFound;
  _draggedTab = nil;
}

- (BOOL)isReorderingTabs {
  return _isReordering;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)recognizer {
  DCHECK([recognizer isKindOfClass:[UILongPressGestureRecognizer class]]);

  // If a drag is already in progress, do not allow another to start.
  return ![self isReorderingTabs];
}

#pragma mark -
#pragma mark Autoscroll methods

- (void)installAutoscrollTimerIfNeeded {
  if (_autoscrollTimer)
    return;

  _autoscrollTimer =
      [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                       target:self
                                     selector:@selector(autoscrollTimerFired:)
                                     userInfo:nil
                                      repeats:YES];
}

- (void)removeAutoscrollTimer {
  [_autoscrollTimer invalidate];
  _autoscrollTimer = nil;
}

- (void)autoscrollTimerFired:(NSTimer*)timer {
  [self constrainAutoscrollDistance];

  CGPoint offset = [_tabStripView contentOffset];
  offset.x += _autoscrollDistance;
  [_tabStripView setContentOffset:offset];

  // Fixed-position views need to have their frames adusted to compensate for
  // the content offset shift.  These include the dragged tab, the dimming
  // view, and the new tab button.
  CGRect tabFrame = [_draggedTab frame];
  tabFrame.origin.x += _autoscrollDistance;
  [_draggedTab setFrame:tabFrame];

  CGRect dimFrame = [_dimmingView frame];
  dimFrame.origin.x += _autoscrollDistance;
  [_dimmingView setFrame:dimFrame];

  // Even though the new tab button is hidden during drag and drop, keep its
  // frame updated to prevent it from animating back into place when the drag
  // finishes.
  CGRect newTabFrame = [_buttonNewTab frame];
  newTabFrame.origin.x += _autoscrollDistance;
  [_buttonNewTab setFrame:newTabFrame];

  // TODO(rohitrao): Find a good way to re-enable the sliding over animation
  // when autoscrolling.  Right now any running animations are immediately
  // stopped by the next call to autoscrollTimerFired.
  [_tabStripView setNeedsLayout];
}

- (void)computeAutoscrollDistanceForTabView:(TabView*)view {
  CGRect scrollBounds = [_tabStripView bounds];
  CGRect viewFrame = [view frame];

  // The distance between this tab and the edge of the scroll view.
  CGFloat distanceFromEdge =
      MIN(CGRectGetMinX(viewFrame) - CGRectGetMinX(scrollBounds),
          CGRectGetMaxX(scrollBounds) - CGRectGetMaxX(viewFrame));
  if (distanceFromEdge < 0)
    distanceFromEdge = 0;

  // Negative if the tab is closer to the left edge of the scroll view, positive
  // if it is closer to the right edge.
  CGFloat leftRightMultiplier =
      (CGRectGetMidX(viewFrame) < CGRectGetMidX(scrollBounds)) ? -1.0 : 1.0;

  // The autoscroll distance decreases linearly as the tab view gets further
  // from the edge of the scroll view.
  _autoscrollDistance =
      leftRightMultiplier *
      MAX(0.0, ceilf(kMaxAutoscrollDistance -
                     distanceFromEdge / kAutoscrollDecrementWidth));
}

- (void)constrainAutoscrollDistance {
  // Make sure autoscroll distance is not so large as to cause overscroll.
  CGPoint offset = [_tabStripView contentOffset];

  // Check to make sure there is no overscroll off the right edge.
  CGFloat maxOffset = [_tabStripView contentSize].width -
                      CGRectGetWidth([_tabStripView bounds]);
  if (offset.x + _autoscrollDistance > maxOffset)
    _autoscrollDistance = (maxOffset - offset.x);

  // Perform the left edge check after the right edge check, to prevent
  // right-justifying the tabs when there is no overflow.
  if (offset.x + _autoscrollDistance < 0)
    _autoscrollDistance = -offset.x;
}

#pragma mark -
#pragma mark - CRWWebStateObserver methods

- (void)webStateDidStartLoading:(web::WebState*)webState {
  // webState can start loading before  didInsertWebState is called, in that
  // case early return as there is no view to update yet.
  if (_tabModel.count > _tabArray.count - _closingTabs.count)
    return;

  if (IsVisibleURLNewTabPage(webState))
    return;

  TabView* view = [self tabViewForWebState:webState];
  if (!view) {
    DCHECK(false) << "Received start loading notification for a Webstate "
                  << "that is not contained in the WebStateList";
    return;
  }
  [view startProgressSpinner];
  [view setNeedsDisplay];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  TabView* view = [self tabViewForWebState:webState];
  if (!view) {
    DCHECK(false) << "Received stop loading notification for a Webstate "
                  << "that is not contained in the WebStateList";
    return;
  }
  // In new Tab case WebState's DidChangeTitle is not called. Make sure to
  // updated the title here to account for that.
  [view setTitle:tab_util::GetTabTitle(webState)];

  [view stopProgressSpinner];
  [view setNeedsDisplay];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  TabView* view = [self tabViewForWebState:webState];
  if (!view) {
    DCHECK(false) << "Received title change notification for a Webstate "
                  << "that is not contained in the WebStateList";
    return;
  }
  [view setTitle:tab_util::GetTabTitle(webState)];
  [view setNeedsDisplay];
}

#pragma mark -
#pragma mark WebStateListObserving methods

// Observer method, active WebState changed.
- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  if (!newWebState)
    return;

  for (TabView* view in _tabArray) {
    [view setSelected:NO];
  }

  NSUInteger index = [self indexForModelIndex:atIndex];
  TabView* activeView = [_tabArray objectAtIndex:index];
  [activeView setSelected:YES];

  // No need to animate this change, as selecting a new tab simply changes the
  // z-ordering of the TabViews.  If a new tab was selected as a result of a tab
  // closure, then the animated layout has already been scheduled.
  [_tabStripView setNeedsLayout];
}

// Observer method. |webState| moved in |webStateList|.
- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  DCHECK(!_isReordering);

  // Reorder the objects in _tabArray to keep in sync with the model ordering.
  NSUInteger arrayIndex = [self indexForModelIndex:fromIndex];
  TabView* view = [_tabArray objectAtIndex:arrayIndex];
  [_tabArray removeObject:view];
  [_tabArray insertObject:view atIndex:toIndex];
  [self setNeedsLayoutWithAnimation];
}

// Observer method, |webState| removed from |webStateList|.
- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  // Keep the actual view around while it is animating out.  Once the animation
  // is done, remove the view.
  NSUInteger index = [self indexForModelIndex:atIndex];
  TabView* view = [_tabArray objectAtIndex:index];
  [_closingTabs addObject:view];
  _targetFrames.RemoveFrame(view);

  // Adjust the content size now that the tab has been removed from the model.
  [self updateContentSizeAndRepositionViews];

  // Signal the FullscreenController that the toolbar needs to stay on
  // screen for a bit, so the animation is visible.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kWillStartTabStripTabAnimation
                    object:nil];

  // Leave the view where it is horizontally and animate it downwards out of
  // sight.
  CGRect frame = [view frame];
  frame = CGRectOffset(frame, 0, CGRectGetHeight(frame));
  [UIView animateWithDuration:kTabAnimationDuration
      animations:^{
        [view setFrame:frame];
      }
      completion:^(BOOL finished) {
        [view removeFromSuperview];
        [_tabArray removeObject:view];
        [_closingTabs removeObject:view];
      }];

  [self setNeedsLayoutWithAnimation];

  [self updateTabCount];
}

// Observer method. |webState| inserted on |webStateList|.
- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  TabView* view = [self createTabViewForWebState:webState
                                      isSelected:activating];
  [_tabArray insertObject:view atIndex:[self indexForModelIndex:index]];
  [[self tabStripView] addSubview:view];

  [self updateContentSizeAndRepositionViews];
  [self setNeedsLayoutWithAnimation];
  [self updateContentOffsetForWebStateIndex:index isNewWebState:YES];

  [self updateTabCount];
}

// Observer method, WebState replaced in |webStateList|.
- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  TabView* view = [self tabViewForWebState:newWebState];
  [self updateTabView:view withWebState:newWebState];
}

#pragma mark -
#pragma mark WebStateFaviconDriverObserver

// Observer method. |webState| got a favicon update.
- (void)faviconDriver:(favicon::FaviconDriver*)driver
    didUpdateFaviconForWebState:(web::WebState*)webState {
  if (!driver)
    return;

  int modelIndex = _tabModel.webStateList->GetIndexOfWebState(webState);
  if (modelIndex == WebStateList::kInvalidIndex) {
    DCHECK(false) << "Received FavIcon update notification for webState that is"
                     " not in the WebStateList";
    return;
  }

  NSUInteger index = [self indexForModelIndex:modelIndex];
  TabView* view = [_tabArray objectAtIndex:index];
  [view setFavicon:nil];

  if (driver->FaviconIsValid()) {
    gfx::Image favicon = driver->GetFavicon();
    if (!favicon.IsEmpty())
      [view setFavicon:favicon.ToUIImage()];
  }
}

#pragma mark -
#pragma mark Views and Layout

- (TabView*)tabViewForWebState:(web::WebState*)webState {
  int modelIndex = _tabModel.webStateList->GetIndexOfWebState(webState);
  if (modelIndex == WebStateList::kInvalidIndex)
    return nil;
  NSUInteger index = [self indexForModelIndex:modelIndex];
  return [_tabArray objectAtIndex:index];
}

- (CGFloat)tabStripVisibleSpace {
  CGFloat availableSpace = CGRectGetWidth([_tabStripView bounds]) -
                           CGRectGetWidth([_buttonNewTab frame]) +
                           kNewTabOverlap;
  return availableSpace;
}

- (void)installTabSwitcherButton {
  DCHECK(!_tabSwitcherButton);
  UIImage* tabSwitcherButtonIcon;
  UIImage* tabSwitcherButtonIconPressed;
  tabSwitcherButtonIcon =
      [UIImage imageNamed:@"tabstrip_tab_switcher_count_button"];
  tabSwitcherButtonIconPressed =
      [UIImage imageNamed:@"tabstrip_tab_switcher_count_button_pressed"];

  int tabSwitcherButtonIdsAccessibilityLabel =
      IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER;
  NSString* tabSwitcherButtonEnglishUiAutomationName = @"Enter Tab Switcher";
  const CGFloat tabStripHeight = _view.frame.size.height;
  CGRect buttonFrame =
      CGRectMake(CGRectGetMaxX(_view.frame) - kTabSwitcherButtonWidth, 0,
                 kTabSwitcherButtonWidth, tabStripHeight);
  _tabSwitcherButton =
      [TabStripCenteredButton buttonWithType:UIButtonTypeCustom];
  if (UseRTLLayout())
    [_tabSwitcherButton setTransform:CGAffineTransformMakeScale(-1, 1)];
  [self addTabSwitcherLongPressGesture];

  [_tabSwitcherButton setTintColor:[UIColor whiteColor]];
  [_tabSwitcherButton setFrame:buttonFrame];
  [_tabSwitcherButton setContentMode:UIViewContentModeCenter];
  [_tabSwitcherButton setAutoresizingMask:UIViewAutoresizingFlexibleLeftMargin];
  [_tabSwitcherButton setBackgroundColor:[UIColor clearColor]];
  [_tabSwitcherButton setExclusiveTouch:YES];
  [_tabSwitcherButton setImage:tabSwitcherButtonIcon
                      forState:UIControlStateNormal];
  [_tabSwitcherButton setImage:tabSwitcherButtonIconPressed
                      forState:UIControlStateHighlighted];
  [_tabSwitcherButton addTarget:self.dispatcher
                         action:@selector(prepareTabSwitcher)
               forControlEvents:UIControlEventTouchDown];
  [_tabSwitcherButton addTarget:self.dispatcher
                         action:@selector(displayTabSwitcher)
               forControlEvents:UIControlEventTouchUpInside];
  [_tabSwitcherButton addTarget:self
                         action:@selector(recordUserMetrics:)
               forControlEvents:UIControlEventTouchUpInside];
  [self updateTabCount];

  SetA11yLabelAndUiAutomationName(_tabSwitcherButton,
                                  tabSwitcherButtonIdsAccessibilityLabel,
                                  tabSwitcherButtonEnglishUiAutomationName);
  [_view addSubview:_tabSwitcherButton];
  // Shrink the scroll view.
  [self updateScrollViewFrameForTabSwitcherButton];
}

// Adds a LongPressGesture to the |_tabSwitcherButton|, with target on
// -|handleTabSwitcherLongPress:|.
- (void)addTabSwitcherLongPressGesture {
  UILongPressGestureRecognizer* longPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleTabSwitcherLongPress:)];
  [_tabSwitcherButton addGestureRecognizer:longPress];
}

// Handles the long press on the |_tabSwitcherButton|.
- (void)handleTabSwitcherLongPress:(UILongPressGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self.dispatcher showTabStripTabGridButtonPopup];
    TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleMedium);
  } else if (gesture.state == UIGestureRecognizerStateEnded) {
    [self.longPressDelegate
        longPressEndedAtPoint:[gesture locationOfTouch:0 inView:nil]];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    [self.longPressDelegate
        longPressFocusPointChangedTo:[gesture locationOfTouch:0 inView:nil]];
  }
}

- (void)shiftTabStripSubviews:(CGPoint)oldContentOffset {
  CGFloat dx = [_tabStripView contentOffset].x - oldContentOffset.x;
  for (UIView* view in [_tabStripView subviews]) {
    CGRect frame = [view frame];
    frame.origin.x += dx;
    [view setFrame:frame];
    _targetFrames.AddFrame(view, frame);
  }
}

- (void)updateContentSizeAndRepositionViews {
  // TODO(rohitrao): The following lines are duplicated in
  // layoutTabStripSubviews.  Find a way to consolidate this logic.
  const NSUInteger tabCount = [_tabArray count] - [_closingTabs count];
  if (!tabCount)
    return;
  const CGFloat tabHeight = CGRectGetHeight([_tabStripView bounds]);
  CGFloat visibleSpace = [self tabStripVisibleSpace];
  _currentTabWidth =
      (visibleSpace + ([self tabOverlap] * (tabCount - 1))) / tabCount;
  _currentTabWidth = MIN(_currentTabWidth, [self maxTabWidth]);
  _currentTabWidth = MAX(_currentTabWidth, [self minTabWidth]);

  // Set the content size to be large enough to contain all the tabs at the
  // desired width, with the standard overlap, plus the new tab button.
  CGSize contentSize = CGSizeMake(
      _currentTabWidth * tabCount - ([self tabOverlap] * (tabCount - 1)) +
          CGRectGetWidth([_buttonNewTab frame]) - kNewTabOverlap,
      tabHeight);
  if (CGSizeEqualToSize([_tabStripView contentSize], contentSize))
    return;

  // Background: The scroll view might change the content offset when updating
  // the content size.  This can happen when the old content offset would result
  // in an overscroll at the new content size.  (Note that the content offset
  // will never change if the content size is growing.)
  //
  // To handle this without making views appear to jump, shift all of the
  // subviews by an amount equal to the size change.
  CGPoint oldOffset = [_tabStripView contentOffset];
  [_tabStripView setContentSize:contentSize];
  [self shiftTabStripSubviews:oldOffset];
}

- (CGRect)scrollViewFrameForTab:(TabView*)view {
  NSUInteger index = [self modelIndexForTabView:view];

  CGRect frame = [view frame];
  frame.origin.x =
      (_currentTabWidth * index) - ([self tabOverlap] * (index - 1));
  return frame;
}

- (CGRect)calculateVisibleFrameForFrame:(CGRect)frame
                         whenUnderFrame:(CGRect)frameOnTop {
  CGFloat minX = CGRectGetMinX(frame);
  CGFloat maxX = CGRectGetMaxX(frame);

  if (CGRectGetMinX(frame) < CGRectGetMinX(frameOnTop))
    maxX = CGRectGetMinX(frameOnTop);
  else
    minX = CGRectGetMaxX(frameOnTop);

  frame.origin.x = minX;
  frame.size.width = maxX - minX;
  return frame;
}

#pragma mark -
#pragma mark - Unstacked layout

- (NSUInteger)maxNumCollapsedTabs {
  return self.useTabStacking ? kMaxNumCollapsedTabsStacked
                             : kMaxNumCollapsedTabsUnstacked;
}

- (CGFloat)tabOverlap {
  return self.useTabStacking ? kTabOverlapStacked : kTabOverlapUnstacked;
}

- (CGFloat)maxTabWidth {
  return self.useTabStacking ? kMaxTabWidthStacked : kMaxTabWidthUnstacked;
}

- (CGFloat)minTabWidth {
  return self.useTabStacking ? kMinTabWidthStacked : kMinTabWidthUnstacked;
}

- (void)scrollTabToVisible:(NSUInteger)tabIndex {
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(tabIndex));

  // The following code calculates the amount of scroll needed to make
  // |tabIndex| visible in the "virtual" coordinate system, where root is x=0
  // and it contains all the tabs laid out as if the tabstrip was infinitely
  // long. The amount of scroll is calculated as a desired length that it is
  // just large enough to contain all the tabs to the left of |tabIndex|, with
  // the standard overlap.
  if (tabIndex == [_tabArray count] - 1) {
    const CGFloat tabStripAvailableSpace =
        _tabStripView.frame.size.width - _tabStripView.contentInset.right;
    CGPoint oldOffset = [_tabStripView contentOffset];
    if (_tabStripView.contentSize.width > tabStripAvailableSpace) {
      CGFloat scrollToPoint =
          _tabStripView.contentSize.width - tabStripAvailableSpace;
      [_tabStripView setContentOffset:CGPointMake(scrollToPoint, 0)];
    }

    // To handle content offset change without making views appear to jump,
    // shift all of the subviews by an amount equal to the size change.
    [self shiftTabStripSubviews:oldOffset];
    return;
  }

  NSUInteger numNonClosingTabsToLeft = 0;
  NSUInteger i = 0;
  for (TabView* tab in _tabArray) {
    if ([_closingTabs containsObject:tab])
      ++i;

    if (i == tabIndex)
      break;

    ++numNonClosingTabsToLeft;
    ++i;
  }

  const CGFloat tabHeight = CGRectGetHeight([_tabStripView bounds]);
  CGRect scrollRect =
      CGRectMake(_currentTabWidth * numNonClosingTabsToLeft -
                     ([self tabOverlap] * (numNonClosingTabsToLeft - 1)),
                 0, _currentTabWidth, tabHeight);
  [_tabStripView scrollRectToVisible:scrollRect animated:YES];
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  nil);
}

- (void)updateContentOffsetForWebStateIndex:(int)webStateIndex
                              isNewWebState:(BOOL)isNewWebState {
  DCHECK_NE(WebStateList::kInvalidIndex, webStateIndex);

  if (isNewWebState) {
    [self scrollTabToVisible:webStateIndex];
    return;
  }

  if (!self.useTabStacking) {
    if (webStateIndex == static_cast<int>([_tabArray count]) - 1) {
      const CGFloat tabStripAvailableSpace =
          _tabStripView.frame.size.width - _tabStripView.contentInset.right;
      if (_tabStripView.contentSize.width > tabStripAvailableSpace) {
        CGFloat scrollToPoint =
            _tabStripView.contentSize.width - tabStripAvailableSpace;
        [_tabStripView setContentOffset:CGPointMake(scrollToPoint, 0)
                               animated:YES];
      }
    } else {
      TabView* tabView = [_tabArray objectAtIndex:webStateIndex];
      CGRect scrollRect =
          CGRectInset(tabView.frame, -_tabStripView.contentInset.right, 0);
      if (tabView)
        [_tabStripView scrollRectToVisible:scrollRect animated:YES];
    }
  }
}

- (void)updateScrollViewFrameForTabSwitcherButton {
  CGRect tabFrame = _tabStripView.frame;
  tabFrame.size.width = _view.bounds.size.width;
  if (self.useTabStacking) {
    tabFrame.size.width -= kTabSwitcherButtonWidth;
    _tabStripView.contentInset = UIEdgeInsetsZero;
    [_tabSwitcherButtonBackgroundView setHidden:YES];
  } else {
    if (!_tabSwitcherButtonBackgroundView) {
      _tabSwitcherButtonBackgroundView = [[UIImageView alloc] init];
      const CGFloat tabStripHeight = _view.frame.size.height;
      const CGRect backgroundViewFrame = CGRectMake(
          CGRectGetMaxX(_view.frame) - kTabSwitcherButtonBackgroundWidth, 0.0,
          kTabSwitcherButtonBackgroundWidth, tabStripHeight);
      [_tabSwitcherButtonBackgroundView setFrame:backgroundViewFrame];
      [_tabSwitcherButtonBackgroundView
          setAutoresizingMask:UIViewAutoresizingFlexibleLeftMargin];
      UIImage* backgroundTabSwitcherImage =
          [UIImage imageNamed:@"tabstrip_toggle_button_gradient"];
      [_tabSwitcherButtonBackgroundView setImage:backgroundTabSwitcherImage];
      [_view addSubview:_tabSwitcherButtonBackgroundView];
    }
    [_tabSwitcherButtonBackgroundView setHidden:NO];
    _tabStripView.contentInset =
        UIEdgeInsetsMake(0, 0, 0, kTabSwitcherButtonWidth);
    [_view bringSubviewToFront:_tabSwitcherButton];
  }
  [_tabStripView setFrame:tabFrame];
}

- (void)updateTabCount {
  [_tabSwitcherButton setTitle:TextForTabCount(_tabModel.count)
                      forState:UIControlStateNormal];
  [_tabSwitcherButton
      setAccessibilityValue:[NSString
                                stringWithFormat:@"%zd", _tabModel.count]];
}

#pragma mark - TabStripViewLayoutDelegate

// Creates TabViews for each Tab in the TabModel and positions them in the
// correct location onscreen.
- (void)layoutTabStripSubviews {
  [self updateTabSwitcherGuide];
  const NSUInteger tabCount = [_tabArray count] - [_closingTabs count];
  if (!tabCount)
    return;
  BOOL animate = _animateLayout;
  _animateLayout = NO;
  // Disable the animation if the tab count is changing from 0 to 1.
  if (tabCount == 1 && [_closingTabs count] == 0)
    animate = NO;

  const CGFloat tabHeight = CGRectGetHeight([_tabStripView bounds]);

  // In unstacked mode the space used to layout the tabs is not constrained and
  // uses the whole scroll view content size width. In stacked mode the
  // available space is constrained to the visible space.
  CGFloat availableSpace = self.useTabStacking
                               ? [self tabStripVisibleSpace]
                               : _tabStripView.contentSize.width;

  // The array and model indexes of the selected tab.
  NSUInteger selectedModelIndex =
      static_cast<NSUInteger>(_tabModel.webStateList->active_index());
  NSUInteger selectedArrayIndex = [self indexForModelIndex:selectedModelIndex];

  // This method lays out tabs in two coordinate systems.  The first, the
  // "virtual" coordinate system, is a system rooted at x=0 that contains all
  // the tabs laid out as if the tabstrip was infinitely long.  In this system,
  // |virtualMinX| contains the starting X coordinate of the next tab to be
  // placed and |virtualMaxX| contains the maximum X coordinate of the last tab
  // to be placed.
  //
  // The scroll view's content area is sized to be large enough to hold all the
  // tabs with proper overlap, but the viewport is set to only show a part of
  // the content area.  The specific part that is shown is given by the scroll
  // view's contentOffset.
  //
  // To layout tabs, first calculate where the tab should be in the "virtual"
  // coordinate system.  This gives the frame of the tab assuming the tabstrip
  // was large enough to hold all tabs without needing to overflow.  Then,
  // adjust the tab's virtual frame to move it onscreen.  This gives the tab's
  // real frame.
  CGFloat virtualMinX = 0;
  CGFloat virtualMaxX = 0;
  CGFloat offset = self.useTabStacking ? [_tabStripView contentOffset].x : 0;

  // Keeps track of which tabs need to be animated.  Using an autoreleased array
  // instead of scoped_nsobject because scoped_nsobject doesn't seem to work
  // well with blocks.
  NSMutableArray* tabsNeedingAnimation =
      [NSMutableArray arrayWithCapacity:tabCount];

  CGRect dragFrame = [_draggedTab frame];

  TabView* previousTabView = nil;
  CGRect previousTabFrame = CGRectZero;
  BOOL hasPlaceholderGap = NO;
  for (NSUInteger arrayIndex = 0; arrayIndex < [_tabArray count];
       ++arrayIndex) {
    TabView* view = (TabView*)[_tabArray objectAtIndex:arrayIndex];

    // Arrange the tabs in a V going backwards from the selected tab.  This
    // differs from desktop in order to make the tab overflow behavior work (on
    // desktop, the tabs are arranged going backwards from left to right, with
    // the selected tab above all others).
    //
    // When reordering, use slightly different logic.  Instead of a V based on
    // the model indexes of the tabs, the V fans out from the placeholder gap,
    // which is visually where the dragged tab is.  In reordering mode, the tabs
    // are not necessarily z-ordered according to their model indexes, because
    // they are not necessarily drawn in the spot dictated by their current
    // model index.
    BOOL isSelectedTab = (arrayIndex == selectedArrayIndex);
    BOOL zOrderedAbove =
        _isReordering ? !hasPlaceholderGap : (arrayIndex <= selectedArrayIndex);

    if (isSelectedTab) {
      // Order matters.  The dimming view needs to end up behind the selected
      // tab, so it's brought to the front first, followed by the tab.
      [_tabStripView bringSubviewToFront:_dimmingView];
      [_tabStripView bringSubviewToFront:view];
    } else if (zOrderedAbove) {
      // If the current tab comes after the selected tab in the model but still
      // needs to be z-ordered above, place it relative to the dimming view,
      // rather than blindly bringing it to the front.  This can only happen in
      // reordering mode.
      if (arrayIndex > selectedArrayIndex) {
        DCHECK(_isReordering);
        [_tabStripView insertSubview:view belowSubview:_dimmingView];
      } else {
        [_tabStripView bringSubviewToFront:view];
      }
    } else {
      [_tabStripView sendSubviewToBack:view];
    }

    // Ignore closing tabs when repositioning.
    NSUInteger currentModelIndex = [self modelIndexForIndex:arrayIndex];
    if (currentModelIndex == NSNotFound)
      continue;

    // Ignore the tab that is currently being dragged.
    if (_isReordering && view == _draggedTab)
      continue;

    // |realMinX| is the furthest left the tab can be, in real coordinates.
    // This is computed by counting the number of possible collapsed tabs that
    // can be to the left of this tab, then multiplying that count by the size
    // of a collapsed tab.
    //
    // There can be up to |[self maxNumCollapsedTabs]| to the left of the
    // selected
    // tab, and the same number to the right of the selected tab.
    NSUInteger numPossibleCollapsedTabsToLeft =
        std::min(currentModelIndex, [self maxNumCollapsedTabs]);
    if (currentModelIndex > selectedModelIndex) {
      // If this tab is to the right of the selected tab, also include the
      // number of collapsed tabs on the right of the selected tab.
      numPossibleCollapsedTabsToLeft =
          std::min(selectedModelIndex, [self maxNumCollapsedTabs]) +
          std::min(currentModelIndex - selectedModelIndex,
                   [self maxNumCollapsedTabs]);
    }
    CGFloat realMinX =
        offset + (numPossibleCollapsedTabsToLeft * kCollapsedTabOverlap);

    // |realMaxX| is the furthest right the tab can be, in real coordinates.
    NSUInteger numPossibleCollapsedTabsToRight =
        std::min(tabCount - currentModelIndex - 1, [self maxNumCollapsedTabs]);
    if (currentModelIndex < selectedModelIndex) {
      // If this tab is to the left of the selected tab, also include the
      // number of collapsed tabs on the left of the selected tab.
      numPossibleCollapsedTabsToRight =
          std::min(tabCount - selectedModelIndex - 1,
                   [self maxNumCollapsedTabs]) +
          std::min(selectedModelIndex - currentModelIndex,
                   [self maxNumCollapsedTabs]);
    }
    CGFloat realMaxX = offset + availableSpace -
                       (numPossibleCollapsedTabsToRight * kCollapsedTabOverlap);

    // If this tab is to the right of the currently dragged tab, add a
    // placeholder gap.
    if (_isReordering && !hasPlaceholderGap &&
        CGRectGetMinX(dragFrame) < virtualMinX + (_currentTabWidth / 2.0)) {
      virtualMinX += _currentTabWidth - [self tabOverlap];
      hasPlaceholderGap = YES;

      // Fix up the z-ordering of the current view.  It was placed assuming that
      // the placeholder gap hasn't been hit yet.
      [_tabStripView sendSubviewToBack:view];

      // The model index of the placeholder gap is equal to the model index of
      // the shifted tab, adjusted for the presence of the dragged tab.  This
      // value will be used as the new model index for the dragged tab when it
      // is dropped.
      _placeholderGapModelIndex = currentModelIndex;
      if ([self modelIndexForTabView:_draggedTab] < currentModelIndex)
        _placeholderGapModelIndex--;
    }

    // |tabX| stores where we are placing the tab, in real coordinates.  Start
    // by trying to place the tab at the computed |virtualMinX|, then constrain
    // that by |realMinX| and |realMaxX|.
    CGFloat tabX = MAX(virtualMinX, realMinX);
    if (tabX + _currentTabWidth > realMaxX)
      tabX = realMaxX - _currentTabWidth;

    CGRect frame = CGRectMake(AlignValueToPixel(tabX), 0,
                              AlignValueToPixel(_currentTabWidth), tabHeight);
    virtualMinX += (_currentTabWidth - [self tabOverlap]);
    virtualMaxX = CGRectGetMaxX(frame);

// TODO(rohitrao): Temporarily disabled this logic as it does not play well with
// tab scrolling.
#if 0
    // If this tab is completely hidden by the previous tab, remove it from the
    // scroll view.  Otherwise, add it back in if needed.
    if (selectedArrayIndex != arrayIndex &&
        CGRectEqualToRect(frame, previousTabFrame)) {
      [view removeFromSuperview];
    } else if (![view superview]) {
      // TODO(rohitrao): Find a way to move the z-ordering code from the top of
      // the function to down here, so we can consolidate the logic.
      [_tabStripView insertSubview:view atIndex:
                       (zOrderedAbove ? [[_tabStripView subviews] count] : 0)];
    }
#endif

    // Update the tab's collapsed state based on overlap with the previous tab.
    if (zOrderedAbove) {
      CGRect visibleRect = [self calculateVisibleFrameForFrame:previousTabFrame
                                                whenUnderFrame:frame];
      BOOL collapsed =
          CGRectGetWidth(visibleRect) < kCollapsedTabWidthThreshold;
      [previousTabView setCollapsed:collapsed];

      // The selected tab can never be collapsed, since no tab will ever be
      // z-ordered above it to obscure it.
      if (isSelectedTab)
        [view setCollapsed:NO];
    } else {
      CGRect visibleRect =
          [self calculateVisibleFrameForFrame:frame
                               whenUnderFrame:previousTabFrame];
      BOOL collapsed =
          CGRectGetWidth(visibleRect) < kCollapsedTabWidthThreshold;
      [view setCollapsed:collapsed];
    }

    if (animate) {
      if (!CGRectEqualToRect(frame, [view frame]))
        [tabsNeedingAnimation addObject:view];
    } else {
      if (!CGRectEqualToRect(frame, [view frame]))
        [view setFrame:frame];
    }

    // Throw the target frame into the dictionary so we can animate it later.
    _targetFrames.AddFrame(view, frame);

    // Ensure the tab is visible.
    if ([view isHidden]) {
      if (animate) {
        // If it is a new tab, and animation is enabled, make it a submarine tab
        // by immediately positioning it under the tabstrip.
        CGRect submarineFrame = CGRectOffset(frame, 0, CGRectGetHeight(frame));
        [view setFrame:submarineFrame];
      }
      [view setHidden:NO];
    }

    previousTabView = view;
    previousTabFrame = frame;
  }

  // If in reordering mode and there was no placeholder gap, then the dragged
  // tab must be all the way to the right of the other tabs.  Set the
  // _placeholderGapModelIndex accordingly.
  if (!hasPlaceholderGap && _isReordering)
    _placeholderGapModelIndex = [_tabModel count] - 1;

  // Do not move the new tab button if it is hidden.  This will lead to better
  // animations when exiting drag and drop mode, as the new tab button will not
  // have moved during the drag.
  CGRect newTabFrame = [_buttonNewTab frame];
  BOOL moveNewTab =
      (newTabFrame.origin.x != virtualMaxX) && !_buttonNewTab.hidden;
  newTabFrame.origin = CGPointMake(virtualMaxX - kNewTabOverlap, 0);
  if (!animate && moveNewTab)
    [_buttonNewTab setFrame:newTabFrame];

  if (animate) {
    float delay = 0.0;
    if (![self.presentationProvider isTabStripFullyVisible]) {
      // Move the toolbar to visible and wait for the end of that animation to
      // animate the appearance of the new tab.
      delay = self.animationWaitDuration;
      // Signal the FullscreenController that the toolbar needs to stay on
      // screen for a bit, so the animation is visible.
      [[NSNotificationCenter defaultCenter]
          postNotificationName:kWillStartTabStripTabAnimation
                        object:nil];
    }

    [UIView animateWithDuration:kTabAnimationDuration
                          delay:delay
                        options:UIViewAnimationOptionAllowUserInteraction
                     animations:^{
                       for (TabView* view in tabsNeedingAnimation) {
                         DCHECK(_targetFrames.HasFrame(view));
                         [view setFrame:_targetFrames.GetFrame(view)];
                       }
                       if (moveNewTab)
                         [_buttonNewTab setFrame:newTabFrame];
                     }
                     completion:nil];
  }
}

- (void)setNeedsLayoutWithAnimation {
  _animateLayout = YES;
  [_tabStripView setNeedsLayout];
}

#pragma mark - DropAndNavigateDelegate

- (void)URLWasDropped:(GURL const&)url {
  // Called when a URL is dropped on the new tab button.
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:url
                                    referrer:web::Referrer()
                                 inIncognito:_isIncognito
                                inBackground:NO
                                    appendTo:kLastTab];
  [self.dispatcher openURLInNewTab:command];
}

#pragma mark - TabViewDelegate

// Called when the TabView was tapped.
- (void)tabViewTapped:(TabView*)tabView {
  // Ignore taps while in reordering mode.
  if ([self isReorderingTabs])
    return;

  NSUInteger index = [self modelIndexForTabView:tabView];
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(index));
  if (index == NSNotFound)
    return;

  if (IsIPadIdiom() &&
      (_tabModel.webStateList->active_index() != static_cast<int>(index))) {
    SnapshotTabHelper::FromWebState(_tabModel.webStateList->GetActiveWebState())
        ->UpdateSnapshotWithCallback(nil);
  }
  _tabModel.webStateList->ActivateWebStateAt(static_cast<int>(index));
  [self updateContentOffsetForWebStateIndex:index isNewWebState:NO];
}

// Called when the TabView's close button was tapped.
- (void)tabViewcloseButtonPressed:(TabView*)tabView {
  // Ignore taps while in reordering mode.
  // TODO(crbug.com/754287): We should just hide the close buttons instead.
  if ([self isReorderingTabs])
    return;

  base::RecordAction(UserMetricsAction("MobileTabStripCloseTab"));
  NSUInteger modelIndex = [self modelIndexForTabView:tabView];
  if (modelIndex != NSNotFound)
    [_tabModel closeTabAtIndex:modelIndex];
}

- (void)tabView:(TabView*)tabView receivedDroppedURL:(GURL)url {
  NSUInteger index = [self modelIndexForTabView:tabView];
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(index));
  if (index == NSNotFound)
    return;
  web::WebState* webState = _tabModel.webStateList->GetWebStateAt(index);

  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  webState->GetNavigationManager()->LoadURLWithParams(params);
}

#pragma mark - Tab Stacking

- (BOOL)shouldUseTabStacking {
  BOOL useTabStacking = !IsCompactTablet();
  if (base::FeatureList::IsEnabled(kVoiceOverUnstackedTabstrip) &&
      UIAccessibilityIsVoiceOverRunning()) {
    useTabStacking = NO;
  }
  if (base::FeatureList::IsEnabled(kForceUnstackedTabstrip)) {
    useTabStacking = NO;
  }
  return useTabStacking;
}

- (void)setUseTabStacking:(BOOL)useTabStacking {
  if (_useTabStacking == useTabStacking) {
    return;
  }

  _useTabStacking = useTabStacking;
  [self updateScrollViewFrameForTabSwitcherButton];
  [self updateContentSizeAndRepositionViews];
  int selectedModelIndex = _tabModel.webStateList->active_index();
  if (selectedModelIndex != WebStateList::kInvalidIndex) {
    [self updateContentOffsetForWebStateIndex:selectedModelIndex
                                isNewWebState:NO];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  self.useTabStacking = [self shouldUseTabStacking];
}

- (void)voiceOverStatusDidChange {
  self.useTabStacking = [self shouldUseTabStacking];
}

@end
