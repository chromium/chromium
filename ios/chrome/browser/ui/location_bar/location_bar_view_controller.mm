// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/infobar_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/infobars/badge/infobar_badge_button.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/orchestrator/location_bar_offset_provider.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

typedef NS_ENUM(int, TrailingButtonState) {
  kNoButton = 0,
  kShareButton,
  kVoiceSearchButton,
};

// FullScreen progress threshold in which to toggle between full screen on and
// off mode for the badge view.
const double kFullscreenProgressBadgeViewThreshold = 0.85;

}  // namespace

@interface LocationBarViewController ()
// The injected edit view.
@property(nonatomic, strong) UIView* editView;

// The injected badge view.
@property(nonatomic, strong) UIView* badgeView;

// The view that displays current location when the omnibox is not focused.
@property(nonatomic, strong) LocationBarSteadyView* locationBarSteadyView;

@property(nonatomic, assign) TrailingButtonState trailingButtonState;

// When this flag is YES, the share button will not be displayed in situations
// when it normally is shown. Setting it triggers a refresh of the button
// visibility.
@property(nonatomic, assign) BOOL hideShareButtonWhileOnIncognitoNTP;

// Keeps the share button enabled status. This is necessary to preserve the
// state of the share button if it's temporarily replaced by the voice search
// icon (in iPad multitasking).
@property(nonatomic, assign) BOOL shareButtonEnabled;

// Starts voice search, updating the NamedGuide to be constrained to the
// trailing button.
- (void)startVoiceSearch;

// Displays the long press menu.
- (void)showLongPressMenu:(UILongPressGestureRecognizer*)sender;

@end

@implementation LocationBarViewController
@synthesize editView = _editView;
@synthesize locationBarSteadyView = _locationBarSteadyView;
@synthesize incognito = _incognito;
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;
@synthesize voiceSearchEnabled = _voiceSearchEnabled;
@synthesize trailingButtonState = _trailingButtonState;
@synthesize hideShareButtonWhileOnIncognitoNTP =
    _hideShareButtonWhileOnIncognitoNTP;
@synthesize shareButtonEnabled = _shareButtonEnabled;
@synthesize offsetProvider = _offsetProvider;

#pragma mark - public

- (instancetype)init {
  self = [super init];
  if (self) {
    _locationBarSteadyView = [[LocationBarSteadyView alloc] init];
  }
  return self;
}

- (void)setEditView:(UIView*)editView {
  DCHECK(!self.editView);
  _editView = editView;
}

- (void)setBadgeView:(UIView*)badgeView {
  DCHECK(!self.badgeView);
  _badgeView = badgeView;
}

- (void)switchToEditing:(BOOL)editing {
  self.editView.hidden = !editing;
  self.locationBarSteadyView.hidden = editing;
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.locationBarSteadyView.colorScheme =
      incognito ? [LocationBarSteadyViewColorScheme incognitoScheme]
                : [LocationBarSteadyViewColorScheme standardScheme];
}

- (void)setDispatcher:(id<ActivityServiceCommands,
                          BrowserCommands,
                          ApplicationCommands,
                          LoadQueryCommands>)dispatcher {
  _dispatcher = dispatcher;
}

- (void)setVoiceSearchEnabled:(BOOL)enabled {
  if (_voiceSearchEnabled == enabled) {
    return;
  }
  _voiceSearchEnabled = enabled;
  [self updateTrailingButtonState];
}

- (void)setHideShareButtonWhileOnIncognitoNTP:(BOOL)hide {
  if (_hideShareButtonWhileOnIncognitoNTP == hide) {
    return;
  }
  _hideShareButtonWhileOnIncognitoNTP = hide;
  [self updateTrailingButton];
}

- (void)updateTrailingButtonState {
  BOOL shouldShowVoiceSearch =
      self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassRegular ||
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  if (shouldShowVoiceSearch) {
    if (self.voiceSearchEnabled) {
      self.trailingButtonState = kVoiceSearchButton;
    } else {
      self.trailingButtonState = kNoButton;
    }
  } else {
    self.trailingButtonState = kShareButton;
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  DCHECK(self.badgeView) << "The badge view must be set at this point";
  self.locationBarSteadyView.badgeView = self.badgeView;

  [_locationBarSteadyView.locationButton
             addTarget:self
                action:@selector(locationBarSteadyViewTapped)
      forControlEvents:UIControlEventTouchUpInside];

  UILongPressGestureRecognizer* recognizer =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(showLongPressMenu:)];
  [_locationBarSteadyView.locationButton addGestureRecognizer:recognizer];

  DCHECK(self.editView) << "The edit view must be set at this point";

  [self.view addSubview:self.editView];
  self.editView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.editView, self.view);

  [self.view addSubview:self.locationBarSteadyView];
  self.locationBarSteadyView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.locationBarSteadyView, self.view);

  [self switchToEditing:NO];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self updateTrailingButtonState];
  [super traitCollectionDidChange:previousTraitCollection];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  CGFloat alphaValue = fmax((progress - 0.85) / 0.15, 0);
  CGFloat scaleValue = 0.79 + 0.21 * progress;
  self.locationBarSteadyView.trailingButton.alpha = alphaValue;
  BOOL badgeViewShouldCollapse =
      progress <= kFullscreenProgressBadgeViewThreshold;
  [self.locationBarSteadyView
      setFullScreenCollapsedMode:badgeViewShouldCollapse];
  self.locationBarSteadyView.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^{
    [self updateForFullscreenProgress:finalProgress];
  }];
}

#pragma mark - LocationBarConsumer

- (void)updateLocationText:(NSString*)string clipTail:(BOOL)clipTail {
  [self.locationBarSteadyView setLocationLabelText:string];
  self.locationBarSteadyView.locationLabel.lineBreakMode =
      clipTail ? NSLineBreakByTruncatingTail : NSLineBreakByTruncatingHead;
}

- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText {
  [self.locationBarSteadyView setLocationImage:icon];
  self.locationBarSteadyView.securityLevelAccessibilityString = statusText;
}

// Updates display on the NTP. Note that this is only meaningful on iPad, where
// the location bar is visible after scrolling the fakebox off the page. On
// iPhone, the location bar is not shown on the NTP at all.
- (void)updateForNTP:(BOOL)isNTP {
  if (isNTP) {
    // Display a fake "placeholder".
    NSString* placeholderString =
        l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
    [self.locationBarSteadyView
        setLocationLabelPlaceholderText:placeholderString];
  }
  self.hideShareButtonWhileOnIncognitoNTP = isNTP;
}

- (void)setShareButtonEnabled:(BOOL)enabled {
  _shareButtonEnabled = enabled;
  if (self.trailingButtonState == kShareButton) {
    [self.locationBarSteadyView enableTrailingButton:enabled];
  }
}

#pragma mark - LocationBarAnimatee

- (void)offsetEditViewToMatchSteadyView {
  CGAffineTransform offsetTransform =
      CGAffineTransformMakeTranslation([self targetOffset], 0);
  self.editView.transform = offsetTransform;
}

- (void)resetEditViewOffsetAndOffsetSteadyViewToMatch {
  self.locationBarSteadyView.transform =
      CGAffineTransformMakeTranslation(-self.editView.transform.tx, 0);
  self.editView.transform = CGAffineTransformIdentity;
}

- (void)offsetSteadyViewToMatchEditView {
  CGAffineTransform offsetTransform =
      CGAffineTransformMakeTranslation(-[self targetOffset], 0);
  self.locationBarSteadyView.transform = offsetTransform;
}

- (void)resetSteadyViewOffsetAndOffsetEditViewToMatch {
  self.editView.transform = CGAffineTransformMakeTranslation(
      -self.locationBarSteadyView.transform.tx, 0);
  self.locationBarSteadyView.transform = CGAffineTransformIdentity;
}

- (void)setSteadyViewFaded:(BOOL)hidden {
  self.locationBarSteadyView.alpha = hidden ? 0 : 1;
}

- (void)hideSteadyViewBadgeView {
  [self.locationBarSteadyView displayBadgeView:NO animated:NO];
}

- (void)showSteadyViewBadgeView {
  [self.locationBarSteadyView displayBadgeView:YES animated:NO];
}

- (void)setEditViewFaded:(BOOL)hidden {
  self.editView.alpha = hidden ? 0 : 1;
}

- (void)setEditViewHidden:(BOOL)hidden {
  self.editView.hidden = hidden;
}
- (void)setSteadyViewHidden:(BOOL)hidden {
  self.locationBarSteadyView.hidden = hidden;
}

- (void)resetTransforms {
  // Focus/defocus animations only affect translations and not scale. So reset
  // translation and keep the scale.
  self.editView.transform = CGAffineTransformMake(
      self.editView.transform.a, self.editView.transform.b,
      self.editView.transform.c, self.editView.transform.d, 0, 0);
  self.locationBarSteadyView.transform =
      CGAffineTransformMake(self.locationBarSteadyView.transform.a,
                            self.locationBarSteadyView.transform.b,
                            self.locationBarSteadyView.transform.c,
                            self.locationBarSteadyView.transform.d, 0, 0);
  ;
}

#pragma mark animation helpers

// Computes the target offset for the focus/defocus animation that allows to
// visually match the position of edit and steady views.
- (CGFloat)targetOffset {
  CGFloat offset = [self.offsetProvider
      xOffsetForString:self.locationBarSteadyView.locationLabel.text];

  CGRect labelRect = [self.view
      convertRect:self.locationBarSteadyView.locationLabel.frame
         fromView:self.locationBarSteadyView.locationLabel.superview];
  CGRect textFieldRect = self.editView.frame;

  CGFloat targetOffset = labelRect.origin.x - textFieldRect.origin.x - offset;
  return targetOffset;
}

#pragma mark - private

- (void)locationBarSteadyViewTapped {
  [self.delegate locationBarSteadyViewTapped];
}

- (void)updateTrailingButton {
  // Stop constraining the voice guide to the trailing button if transitioning
  // from kVoiceSearchButton.
  NamedGuide* voiceGuide =
      [NamedGuide guideWithName:kVoiceSearchButtonGuide
                           view:self.locationBarSteadyView];
  if (voiceGuide.constrainedView == self.locationBarSteadyView.trailingButton)
    voiceGuide.constrainedView = nil;


  // Cancel previous possible state.
  [self.locationBarSteadyView.trailingButton
          removeTarget:nil
                action:nil
      forControlEvents:UIControlEventAllEvents];
  self.locationBarSteadyView.trailingButton.hidden = NO;

  TrailingButtonState state = self.trailingButtonState;
  if (state == kShareButton && self.hideShareButtonWhileOnIncognitoNTP) {
    state = kNoButton;
  }

  switch (state) {
    case kNoButton: {
      self.locationBarSteadyView.trailingButton.hidden = YES;
      break;
    };
    case kShareButton: {
      [self.locationBarSteadyView.trailingButton
                 addTarget:self.dispatcher
                    action:@selector(sharePage)
          forControlEvents:UIControlEventTouchUpInside];

      // Add self as a target to collect the metrics.
      [self.locationBarSteadyView.trailingButton
                 addTarget:self
                    action:@selector(shareButtonPressed)
          forControlEvents:UIControlEventTouchUpInside];

      [self.locationBarSteadyView.trailingButton
          setImage:
              [[UIImage imageNamed:@"location_bar_share"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
          forState:UIControlStateNormal];
      self.locationBarSteadyView.trailingButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SHARE);
      [self.locationBarSteadyView enableTrailingButton:self.shareButtonEnabled];
      break;
    };
    case kVoiceSearchButton: {
      [self.locationBarSteadyView.trailingButton
                 addTarget:self.dispatcher
                    action:@selector(preloadVoiceSearch)
          forControlEvents:UIControlEventTouchDown];
      [self.locationBarSteadyView.trailingButton
                 addTarget:self
                    action:@selector(startVoiceSearch)
          forControlEvents:UIControlEventTouchUpInside];
      [self.locationBarSteadyView.trailingButton
          setImage:
              [[UIImage imageNamed:@"location_bar_voice"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal]
          forState:UIControlStateNormal];
      self.locationBarSteadyView.trailingButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_VOICE_SEARCH);
      [self.locationBarSteadyView enableTrailingButton:YES];
    }
  }
}

- (void)setTrailingButtonState:(TrailingButtonState)state {
  if (_trailingButtonState == state) {
    return;
  }
  _trailingButtonState = state;

  [self updateTrailingButton];
}

- (void)startVoiceSearch {
  [NamedGuide guideWithName:kVoiceSearchButtonGuide view:self.view]
      .constrainedView = self.locationBarSteadyView.trailingButton;
  [self.dispatcher startVoiceSearch];
}

// Called when the share button is pressed.
// The actual share dialog is opened by the dispatcher, only collect the metrics
// here.
- (void)shareButtonPressed {
  RecordAction(UserMetricsAction("MobileToolbarShareMenu"));
}


#pragma mark - UIMenu

- (void)showLongPressMenu:(UILongPressGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    [self.locationBarSteadyView becomeFirstResponder];

    // TODO(crbug.com/862583): Investigate why it's necessary to delay showing
    // the editing menu in the omnibox until the next runloop. If it's not
    // delayed by a runloop, the menu appears and is hidden again right away
    // when it's the first time setting the first responder.
    dispatch_async(dispatch_get_main_queue(), ^{
      UIMenuController* menu = [UIMenuController sharedMenuController];
      UIMenuItem* searchCopiedImage = [[UIMenuItem alloc]
          initWithTitle:l10n_util::GetNSString((IDS_IOS_SEARCH_COPIED_IMAGE))
                 action:@selector(searchCopiedImage:)];
      UIMenuItem* visitCopiedLink = [[UIMenuItem alloc]
          initWithTitle:l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK)
                 action:@selector(visitCopiedLink:)];
      UIMenuItem* searchCopiedText = [[UIMenuItem alloc]
          initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT)
                 action:@selector(searchCopiedText:)];
      [menu setMenuItems:@[
        searchCopiedImage, visitCopiedLink, searchCopiedText
      ]];

      [menu setTargetRect:self.locationBarSteadyView.frame inView:self.view];
      [menu setMenuVisible:YES animated:YES];
      // When we present the menu manually, it doesn't get focused by Voiceover.
      // This notification forces voiceover to select the presented menu.
      UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                      menu);
    });
  }
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // Allow copying if the steady location bar is visible.
  if (!self.locationBarSteadyView.hidden && action == @selector(copy:)) {
    return YES;
  }

  if (action == @selector(searchCopiedImage:) ||
      action == @selector(visitCopiedLink:) ||
      action == @selector(searchCopiedText:)) {
    ClipboardRecentContent* clipboardRecentContent =
        ClipboardRecentContent::GetInstance();
    if (self.searchByImageEnabled &&
        clipboardRecentContent->GetRecentImageFromClipboard().has_value()) {
      return action == @selector(searchCopiedImage:);
    }
    if (clipboardRecentContent->GetRecentURLFromClipboard().has_value()) {
      return action == @selector(visitCopiedLink:);
    }
    if (clipboardRecentContent->GetRecentTextFromClipboard().has_value()) {
      return action == @selector(searchCopiedText:);
    }
    return NO;
  }
  return NO;
}

- (void)copy:(id)sender {
  [self.delegate locationBarCopyTapped];
}

- (void)searchCopiedImage:(id)sender {
  RecordAction(
      UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedImage"));
  if (base::Optional<gfx::Image> optionalImage =
          ClipboardRecentContent::GetInstance()
              ->GetRecentImageFromClipboard()) {
    UIImage* image = optionalImage.value().ToUIImage();
    [self.dispatcher searchByImage:image];
  }
}

- (void)visitCopiedLink:(id)sender {
  RecordAction(
      UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedImage"));
  [self pasteAndGo:sender];
}

- (void)searchCopiedText:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedText"));
  [self pasteAndGo:sender];
}

// Both actions are performed the same, but need to be enabled differently,
// so we need two different selectors.
- (void)pasteAndGo:(id)sender {
  NSString* query;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  if (base::Optional<GURL> optionalUrl =
          clipboardRecentContent->GetRecentURLFromClipboard()) {
    query = base::SysUTF8ToNSString(optionalUrl.value().spec());
  } else if (base::Optional<base::string16> optionalText =
                 clipboardRecentContent->GetRecentTextFromClipboard()) {
    query = base::SysUTF16ToNSString(optionalText.value());
  }
  [self.dispatcher loadQuery:query immediately:YES];
}

@end
