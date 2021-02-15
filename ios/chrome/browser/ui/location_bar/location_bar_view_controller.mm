// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_view_controller.h"

#include "base/bind.h"
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
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/orchestrator/location_bar_offset_provider.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/whats_new/default_browser_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
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

// Identifier for the omnibox embedded in this location bar as a scribble
// element.
const NSString* kScribbleOmniboxElementId = @"omnibox";

}  // namespace

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
@interface LocationBarViewController (Scribble) <
    UIIndirectScribbleInteractionDelegate>
@end
#endif  // defined(__IPHONE14_0)

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

// Stores whether the clipboard currently stores copied content.
@property(nonatomic, assign) BOOL hasCopiedContent;
// Stores the current content type in the clipboard. This is only valid if
// |hasCopiedContent| is YES.
@property(nonatomic, assign) ClipboardContentType copiedContentType;
// Stores whether the cached clipboard state is currently being updated. See
// |-updateCachedClipboardState| for more information.
@property(nonatomic, assign) BOOL isUpdatingCachedClipboardState;

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
                          LoadQueryCommands,
                          OmniboxCommands>)dispatcher {
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

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    UIIndirectScribbleInteraction* scribbleInteraction =
        [[UIIndirectScribbleInteraction alloc] initWithDelegate:self];
    [_locationBarSteadyView addInteraction:scribbleInteraction];
  }
#endif  // #if defined(__IPHONE_14_0)

  DCHECK(self.editView) << "The edit view must be set at this point";

  [self.view addSubview:self.editView];
  self.editView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.editView, self.view);

  [self.view addSubview:self.locationBarSteadyView];
  self.locationBarSteadyView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.locationBarSteadyView, self.view);

  [self switchToEditing:NO];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:UIPasteboardChangedNotification
              object:nil];

  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
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

#pragma mark - UIIndirectScribbleInteractionDelegate

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
              requestElementsInRect:(CGRect)rect
                         completion:
                             (void (^)(NSArray<UIScribbleElementIdentifier>*
                                           elements))completion
    API_AVAILABLE(ios(14.0)) {
  completion(@[ kScribbleOmniboxElementId ]);
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                   isElementFocused:
                       (UIScribbleElementIdentifier)elementIdentifier
    API_AVAILABLE(ios(14.0)) {
  DCHECK(elementIdentifier == kScribbleOmniboxElementId);
  return self.delegate.omniboxScribbleForwardingTarget.isFirstResponder;
}

- (CGRect)
    indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                frameForElement:(UIScribbleElementIdentifier)elementIdentifier
    API_AVAILABLE(ios(14.0)) {
  DCHECK(elementIdentifier == kScribbleOmniboxElementId);

  // Imitate the entire location bar being scribblable.
  return self.view.bounds;
}

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
               focusElementIfNeeded:
                   (UIScribbleElementIdentifier)elementIdentifier
                     referencePoint:(CGPoint)focusReferencePoint
                         completion:
                             (void (^)(UIResponder<UITextInput>* focusedInput))
                                 completion API_AVAILABLE(ios(14.0)) {
  if (!self.delegate.omniboxScribbleForwardingTarget.isFirstResponder) {
    [self.delegate locationBarRequestScribbleTargetFocus];
  }

  completion(self.delegate.omniboxScribbleForwardingTarget);
}

#endif  // defined(__IPHONE_14_0)

#pragma mark - private

- (void)locationBarSteadyViewTapped {
  base::RecordAction(base::UserMetricsAction("MobileLocationBarTapped"));
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
      self.locationBarSteadyView.trailingButton.accessibilityIdentifier =
          kOmniboxShareButtonIdentifier;
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
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
          forState:UIControlStateNormal];
      self.locationBarSteadyView.trailingButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_VOICE_SEARCH);
      self.locationBarSteadyView.trailingButton.accessibilityIdentifier =
          kOmniboxVoiceSearchButtonIdentifier;
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

// Updates the cached clipboard content type and calls |completion| when the
// update process is finished.  If this is called while an update is already in
// progress, it will return NO and the completion will never be called.
// Otherwise, returns YES.
- (BOOL)updateCachedClipboardStateWithCompletion:(void (^)(void))completion {
  // Sometimes, checking the clipboard state itself causes the clipboard to
  // emit a UIPasteboardChangedNotification, leading to an infinite loop. For
  // now, just prevent re-checking the clipboard state, but hopefully this will
  // be fixed in a future iOS version (see crbug.com/1049053 for crash details).
  if (self.isUpdatingCachedClipboardState) {
    return NO;
  }
  self.isUpdatingCachedClipboardState = YES;
  self.hasCopiedContent = NO;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  std::set<ClipboardContentType> desired_types;
  desired_types.insert(ClipboardContentType::URL);
  desired_types.insert(ClipboardContentType::Text);
  desired_types.insert(ClipboardContentType::Image);
  __weak __typeof(self) weakSelf = self;
  clipboardRecentContent->HasRecentContentFromClipboard(
      desired_types,
      base::BindOnce(^(std::set<ClipboardContentType> matched_types) {
        weakSelf.hasCopiedContent = !matched_types.empty();
        if (weakSelf.searchByImageEnabled &&
            matched_types.find(ClipboardContentType::Image) !=
                matched_types.end()) {
          weakSelf.copiedContentType = ClipboardContentType::Image;
        } else if (matched_types.find(ClipboardContentType::URL) !=
                   matched_types.end()) {
          weakSelf.copiedContentType = ClipboardContentType::URL;
        } else if (matched_types.find(ClipboardContentType::Text) !=
                   matched_types.end()) {
          weakSelf.copiedContentType = ClipboardContentType::Text;
        }
        weakSelf.isUpdatingCachedClipboardState = NO;
        completion();
      }));
  return YES;
}

#pragma mark - UIMenu

- (void)showLongPressMenu:(UILongPressGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    [self.locationBarSteadyView becomeFirstResponder];

    UIMenuController* menu = [UIMenuController sharedMenuController];
    RegisterEditMenuItem([[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString((IDS_IOS_SEARCH_COPIED_IMAGE))
               action:@selector(searchCopiedImage:)]);
    RegisterEditMenuItem([[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK)
               action:@selector(visitCopiedLink:)]);
    RegisterEditMenuItem([[UIMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT)
               action:@selector(searchCopiedText:)]);

    BOOL updateSuccessful = [self updateCachedClipboardStateWithCompletion:^() {
#if !defined(__IPHONE_13_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_13_0
      [menu setTargetRect:self.locationBarSteadyView.frame inView:self.view];
      [menu setMenuVisible:YES animated:YES];
#else
      [menu showMenuFromView:self.view rect:self.locationBarSteadyView.frame];
#endif
      // When the menu is manually presented, it doesn't get focused by
      // Voiceover. This notification forces voiceover to select the
      // presented menu.
      UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                      menu);
    }];
    DCHECK(updateSuccessful);
  }
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // Allow copying if the steady location bar is visible.
  if (!self.locationBarSteadyView.hidden && action == @selector(copy:)) {
    return YES;
  }

  BOOL isClipboardAction = action == @selector(searchCopiedImage:) ||
                           action == @selector(visitCopiedLink:) ||
                           action == @selector(searchCopiedText:);
  if (self.locationBarSteadyView.isFirstResponder && isClipboardAction) {
    if (!self.hasCopiedContent) {
      return NO;
    }
    if (self.copiedContentType == ClipboardContentType::Image) {
      return action == @selector(searchCopiedImage:);
    }
    if (self.copiedContentType == ClipboardContentType::URL) {
      return action == @selector(visitCopiedLink:);
    }
    if (self.copiedContentType == ClipboardContentType::Text) {
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
  ClipboardRecentContent::GetInstance()->GetRecentImageFromClipboard(
      base::BindOnce(^(base::Optional<gfx::Image> optionalImage) {
        if (!optionalImage) {
          return;
        }
        UIImage* image = optionalImage.value().ToUIImage();
        dispatch_async(dispatch_get_main_queue(), ^{
          [self.dispatcher searchByImage:image];
          [self.dispatcher cancelOmniboxEdit];
        });
      }));
}

- (void)visitCopiedLink:(id)sender {
  // A search using clipboard link is activity that should indicate a user
  // that would be interested in setting Chrome as the default browser.
  LogLikelyInterestedDefaultBrowserUserActivity();
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.VisitCopiedLink"));
  ClipboardRecentContent::GetInstance()->GetRecentURLFromClipboard(
      base::BindOnce(^(base::Optional<GURL> optionalURL) {
        if (!optionalURL) {
          return;
        }
        NSString* url = base::SysUTF8ToNSString(optionalURL.value().spec());
        dispatch_async(dispatch_get_main_queue(), ^{
          [self.dispatcher loadQuery:url immediately:YES];
          [self.dispatcher cancelOmniboxEdit];
        });
      }));
}

- (void)searchCopiedText:(id)sender {
  // A search using clipboard text is activity that should indicate a user
  // that would be interested in setting Chrome as the default browser.
  LogLikelyInterestedDefaultBrowserUserActivity();
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedText"));
  ClipboardRecentContent::GetInstance()->GetRecentTextFromClipboard(
      base::BindOnce(^(base::Optional<base::string16> optionalText) {
        if (!optionalText) {
          return;
        }
        NSString* query = base::SysUTF16ToNSString(optionalText.value());
        dispatch_async(dispatch_get_main_queue(), ^{
          [self.dispatcher loadQuery:query immediately:YES];
          [self.dispatcher cancelOmniboxEdit];
        });
      }));
}

@end
