// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_view_controller.h"

#import "base/containers/contains.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_entrypoint_view.h"
#import "ios/chrome/browser/location_bar/ui_bundled/badges_container_view.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/location_bar_offset_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/text_field_view_containing.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace {

typedef NS_ENUM(int, TrailingButtonState) {
  kNoButton = 0,
  kShareButton,
  kVoiceSearchButton,
};

// The size of the symbol image.
const CGFloat kSymbolImagePointSize = 18.;

// Identifier for the omnibox embedded in this location bar as a scribble
// element.
const NSString* kScribbleOmniboxElementId = @"omnibox";

}  // namespace

@interface LocationBarViewController () <UIContextMenuInteractionDelegate,
                                         UIIndirectScribbleInteractionDelegate>
// The injected edit view.
@property(nonatomic, strong) UIView<TextFieldViewContaining>* editView;

// The injected text field.
@property(nonatomic, weak) UIView* textField;

// The injected badge view.
@property(nonatomic, strong) UIView* badgeView;

// The injected Contextual Panel entrypoint view;
@property(nonatomic, strong) UIView* contextualPanelEntrypointView;

// The injected placeholder view;
@property(nonatomic, strong) UIView* placeholderView;

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

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

// Whether the default search engine supports Lensing images. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL lensImageEnabled;

// Type of the current placeholder view.
@property(nonatomic, assign) LocationBarPlaceholderType placeholderType;

// Starts voice search, updating the layout guide to be constrained to the
// trailing button.
- (void)startVoiceSearch;

@end

@implementation LocationBarViewController {
  BOOL _isNTP;

  LensOverlayEntrypointButton* _lensOverlayPlaceholderView;
}

#pragma mark - public

- (instancetype)init {
  self = [super init];
  if (self) {
    _locationBarSteadyView = [[LocationBarSteadyView alloc] init];
  }
  return self;
}

- (void)setEditView:(UIView<TextFieldViewContaining>*)editView {
  DCHECK(!self.editView);
  _editView = editView;
  _textField = editView.textFieldView;
}

- (void)setBadgeView:(UIView*)badgeView {
  DCHECK(!self.badgeView);
  _badgeView = badgeView;
}

- (void)setContextualPanelEntrypointView:
    (UIView*)contextualPanelEntrypointView {
  DCHECK(!self.contextualPanelEntrypointView);
  _contextualPanelEntrypointView = contextualPanelEntrypointView;
}

- (void)setPlaceholderType:(LocationBarPlaceholderType)placeholderType {
  CHECK(IsLensOverlayAvailable());
  if (placeholderType == _placeholderType) {
    return;
  }
  _placeholderType = placeholderType;
  if (self.isViewLoaded) {
    [self updatePlaceholderView];
  }
}

- (void)switchToEditing:(BOOL)editing {
  self.editView.hidden = !editing;
  self.locationBarSteadyView.hidden = editing;
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.locationBarSteadyView.colorScheme =
      [LocationBarSteadyViewColorScheme standardScheme];
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

- (id<ContextualPanelEntrypointVisibilityDelegate>)
    contextualEntrypointVisibilityDelegate {
  return self.locationBarSteadyView.contextualEntrypointVisibilityDelegate;
}

- (id<BadgeViewVisibilityDelegate>)badgeViewVisibilityDelegate {
  return self.locationBarSteadyView.badgeViewVisibilityDelegate;
}

- (void)setHelpCommandsHandler:(id<HelpCommands>)helpCommandsHandler {
  _helpCommandsHandler = helpCommandsHandler;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.clipsToBounds = YES;

  // TODO(crbug.com/328446957): Cleanup when fully launched, at which point
  // `contextualPanelEntrypointView` should be CHECK()'ed. Until fully launched,
  // the entrypoint view might be nil if the flag is disabled.
  if (self.contextualPanelEntrypointView) {
    [self.locationBarSteadyView
        setContextualPanelEntrypointView:self.contextualPanelEntrypointView];
  }

  DCHECK(self.badgeView) << "The badge view must be set at this point";
  [self.locationBarSteadyView setBadgeView:self.badgeView];

  if (IsLensOverlayAvailable()) {
    _lensOverlayPlaceholderView = [[LensOverlayEntrypointButton alloc] init];
    [self.layoutGuideCenter referenceView:_lensOverlayPlaceholderView
                                underName:kLensOverlayEntrypointGuide];
    [_lensOverlayPlaceholderView addTarget:self
                                    action:@selector(openLensOverlay)
                          forControlEvents:UIControlEventTouchUpInside];
  }

  [_locationBarSteadyView.locationButton
             addTarget:self
                action:@selector(locationBarSteadyViewTapped)
      forControlEvents:UIControlEventTouchUpInside];

  [_locationBarSteadyView
      addInteraction:[[UIContextMenuInteraction alloc] initWithDelegate:self]];

  UIIndirectScribbleInteraction* scribbleInteraction =
      [[UIIndirectScribbleInteraction alloc] initWithDelegate:self];
  [_locationBarSteadyView addInteraction:scribbleInteraction];

  DCHECK(self.editView) << "The edit view must be set at this point";

  [self.view addSubview:self.editView];
  self.editView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.editView, self.view);

  [self.view addSubview:self.locationBarSteadyView];
  self.locationBarSteadyView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.locationBarSteadyView, self.view);

  [self updatePlaceholderView];
  [self updateTrailingButtonState];
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
  self.locationBarSteadyView.badgesContainerView.placeholderView.alpha =
      alphaValue;
  BOOL badgeViewShouldCollapse = progress <= kFullscreenProgressThreshold;
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

- (void)defocusOmnibox {
  [self.dispatcher cancelOmniboxEdit];
}

#pragma mark - LocationBarSteadyViewConsumer

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
  _isNTP = isNTP;
  if (isNTP) {
    // Display a fake "placeholder".
    NSString* placeholderString =
        l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
    [self.locationBarSteadyView
        setLocationLabelPlaceholderText:placeholderString];
  }
  [self.locationBarSteadyView setCentered:(!isNTP || self.incognito)];
  self.hideShareButtonWhileOnIncognitoNTP = isNTP;
}

- (void)setShareButtonEnabled:(BOOL)enabled {
  _shareButtonEnabled = enabled;
  if (self.trailingButtonState == kShareButton) {
    [self.locationBarSteadyView enableTrailingButton:enabled];

    if (_shareButtonEnabled) {
      [self.layoutGuideCenter
          referenceView:self.locationBarSteadyView.trailingButton
              underName:kShareButtonGuide];
    }
  }
}

- (BOOL)canShowLargeContextualPanelEntrypoint {
  // TODO(crbug.com/330701617): Add actual checks when implementing badge view
  // loud moment blocking (check might need to be in the actual view).
  return !self.locationBarSteadyView.hidden;
}

- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered {
  [self.locationBarSteadyView
      setLocationBarLabelCenteredBetweenContent:centered];
}

- (void)attemptShowingLensOverlayIPH {
  if (IsLensOverlayAvailable() &&
      !self.locationBarSteadyView.badgesContainerView.placeholderView.hidden) {
    [self.helpCommandsHandler
        presentInProductHelpWithType:InProductHelpType::kLensOverlayEntrypoint];
  }
}

- (void)recordLensOverlayAvailability {
  // Record lens overlay placeholder available.
  if (_placeholderType == LocationBarPlaceholderType::kLensOverlay) {
    RecordLensEntrypointAvailable();
  }
}

#pragma mark - LocationBarAnimatee

- (void)offsetTextFieldToMatchSteadyView {
  CGAffineTransform offsetTransform =
      CGAffineTransformMakeTranslation([self targetOffset], 0);
  self.textField.transform = offsetTransform;
}

- (void)resetTextFieldOffsetAndOffsetSteadyViewToMatch {
  self.locationBarSteadyView.transform =
      CGAffineTransformMakeTranslation(-self.textField.transform.tx, 0);
  self.textField.transform = CGAffineTransformIdentity;
}

- (void)offsetSteadyViewToMatchTextField {
  CGAffineTransform offsetTransform =
      CGAffineTransformMakeTranslation(-[self targetOffset], 0);
  self.locationBarSteadyView.transform = offsetTransform;
}

- (void)resetSteadyViewOffsetAndOffsetTextFieldToMatch {
  self.textField.transform = CGAffineTransformMakeTranslation(
      -self.locationBarSteadyView.transform.tx, 0);
  self.locationBarSteadyView.transform = CGAffineTransformIdentity;
}

- (void)setSteadyViewFaded:(BOOL)hidden {
  self.locationBarSteadyView.alpha = hidden ? 0 : 1;
}

- (void)hideSteadyViewBadgeAndEntrypointViews {
  self.locationBarSteadyView.badgesContainerView.hidden = YES;
}

- (void)showSteadyViewBadgeAndEntrypointViews {
  self.locationBarSteadyView.badgesContainerView.hidden = NO;
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
  self.textField.transform = CGAffineTransformMake(
      self.textField.transform.a, self.textField.transform.b,
      self.textField.transform.c, self.textField.transform.d, 0, 0);
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
  CGFloat offset =
      _isNTP
          ? kOmniboxEditOffset
          : [self.offsetProvider
                xOffsetForString:self.locationBarSteadyView.locationLabel.text];

  CGRect labelRect = [self.view
      convertRect:self.locationBarSteadyView.locationLabel.frame
         fromView:self.locationBarSteadyView.locationLabel.superview];
  CGRect textFieldRect = self.editView.frame;

  CGFloat targetOffset = labelRect.origin.x - textFieldRect.origin.x - offset;
  return targetOffset;
}

#pragma mark - UIIndirectScribbleInteractionDelegate

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
              requestElementsInRect:(CGRect)rect
                         completion:
                             (void (^)(NSArray<UIScribbleElementIdentifier>*
                                           elements))completion {
  completion(@[ kScribbleOmniboxElementId ]);
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                   isElementFocused:
                       (UIScribbleElementIdentifier)elementIdentifier {
  DCHECK(elementIdentifier == kScribbleOmniboxElementId);
  return self.delegate.omniboxScribbleForwardingTarget.isFirstResponder;
}

- (CGRect)
    indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                frameForElement:(UIScribbleElementIdentifier)elementIdentifier {
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
                                 completion {
  if (!self.delegate.omniboxScribbleForwardingTarget.isFirstResponder) {
    [self.delegate locationBarRequestScribbleTargetFocus];
  }

  completion(self.delegate.omniboxScribbleForwardingTarget);
}

#pragma mark - private

- (void)locationBarSteadyViewTapped {
  base::RecordAction(base::UserMetricsAction("MobileLocationBarTapped"));
  TriggerHapticFeedbackForSelectionChange();
  [self.delegate locationBarSteadyViewTapped];
}

- (void)updateTrailingButton {
  // Stop constraining the voice guide to the trailing button if transitioning
  // from kVoiceSearchButton.
  UIView* referencedView =
      [self.layoutGuideCenter referencedViewUnderName:kVoiceSearchButtonGuide];
  if (referencedView == self.locationBarSteadyView.trailingButton) {
    [self.layoutGuideCenter referenceView:nil
                                underName:kVoiceSearchButtonGuide];
  }

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

      UIImage* shareImage =
          DefaultSymbolWithPointSize(kShareSymbol, kSymbolImagePointSize);
      [self.locationBarSteadyView.trailingButton setImage:shareImage
                                                 forState:UIControlStateNormal];
      self.locationBarSteadyView.trailingButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SHARE);
      self.locationBarSteadyView.trailingButton.accessibilityIdentifier =
          kOmniboxShareButtonIdentifier;
      [self.locationBarSteadyView enableTrailingButton:self.shareButtonEnabled];

      if (self.shareButtonEnabled) {
        [self.layoutGuideCenter
            referenceView:self.locationBarSteadyView.trailingButton
                underName:kShareButtonGuide];
      }
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

      UIImage* micImage =
          DefaultSymbolWithPointSize(kMicrophoneSymbol, kSymbolImagePointSize);
      [self.locationBarSteadyView.trailingButton setImage:micImage
                                                 forState:UIControlStateNormal];
      self.locationBarSteadyView.trailingButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_VOICE_SEARCH);
      self.locationBarSteadyView.trailingButton.accessibilityIdentifier =
          kOmniboxVoiceSearchButtonIdentifier;
      self.locationBarSteadyView.trailingButton.layer.cornerRadius =
          self.locationBarSteadyView.trailingButton.frame.size.width / 2;
      self.locationBarSteadyView.trailingButton.clipsToBounds = YES;

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
  [self.layoutGuideCenter
      referenceView:self.locationBarSteadyView.trailingButton
          underName:kVoiceSearchButtonGuide];
  base::RecordAction(base::UserMetricsAction("MobileOmniboxVoiceSearch"));
  [self.dispatcher startVoiceSearch];
}

// Called when the share button is pressed.
// The actual share dialog is opened by the dispatcher, only collect the metrics
// here.
- (void)shareButtonPressed {
  RecordAction(UserMetricsAction("MobileToolbarShareMenu"));
  [self.delegate recordShareButtonPressed];
}

- (BOOL)shouldUseLensInLongPressMenu {
  return ios::provider::IsLensSupported() &&
         base::FeatureList::IsEnabled(kEnableLensInOmniboxCopiedImage) &&
         self.lensImageEnabled;
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIMenu*)contextMenuUIMenu:(NSArray<UIMenuElement*>*)suggestedActions {
  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  __weak __typeof__(self) weakSelf = self;
  UIImage* pasteImage = nil;
  if (IsBottomOmniboxAvailable()) {
    pasteImage =
        DefaultSymbolWithPointSize(kPasteActionSymbol, kSymbolActionPointSize);

    // Copy link action.
    if (!self.locationBarSteadyView.hidden) {
      UIAction* copyAction = [UIAction
          actionWithTitle:l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE)
                    image:DefaultSymbolWithPointSize(kCopyActionSymbol,
                                                     kSymbolActionPointSize)
               identifier:nil
                  handler:^(UIAction* action) {
                    [weakSelf.delegate locationBarCopyTapped];
                  }];
      [menuElements addObject:copyAction];
    }
  } else {
    // Keep the suggested actions to have the copy action in a separate section.
    [menuElements addObjectsFromArray:suggestedActions];
  }

  std::optional<std::set<ClipboardContentType>> clipboard_content_types =
      ClipboardRecentContent::GetInstance()->GetCachedClipboardContentTypes();

  if (clipboard_content_types.has_value()) {
    std::set<ClipboardContentType> clipboard_content_types_values =
        clipboard_content_types.value();
    if (base::Contains(clipboard_content_types_values,
                       ClipboardContentType::Image)) {
      // Either add an option to search the copied image with Lens, or via the
      // default search engine's reverse image search functionality.
      if (self.shouldUseLensInLongPressMenu) {
        id lensCopiedImageHandler = ^(UIAction* action) {
          [weakSelf lensCopiedImage:nil];
        };
        UIAction* lensCopiedImageAction = [UIAction
            actionWithTitle:l10n_util::GetNSString(
                                (IDS_IOS_SEARCH_COPIED_IMAGE_WITH_LENS))
                      image:pasteImage
                 identifier:nil
                    handler:lensCopiedImageHandler];
        [menuElements addObject:lensCopiedImageAction];
      } else {
        id searchCopiedImageHandler = ^(UIAction* action) {
          [weakSelf searchCopiedImage:nil];
        };
        UIAction* searchCopiedImageAction =
            [UIAction actionWithTitle:l10n_util::GetNSString(
                                          (IDS_IOS_SEARCH_COPIED_IMAGE))
                                image:pasteImage
                           identifier:nil
                              handler:searchCopiedImageHandler];
        [menuElements addObject:searchCopiedImageAction];
      }
    } else if (base::Contains(clipboard_content_types_values,
                              ClipboardContentType::URL)) {
      id visitCopiedLinkHandler = ^(UIAction* action) {
        [self visitCopiedLink:nil];
      };
      UIAction* visitCopiedLinkAction = [UIAction
          actionWithTitle:l10n_util::GetNSString((IDS_IOS_VISIT_COPIED_LINK))
                    image:pasteImage
               identifier:nil
                  handler:visitCopiedLinkHandler];
      [menuElements addObject:visitCopiedLinkAction];
    } else if (base::Contains(clipboard_content_types_values,
                              ClipboardContentType::Text)) {
      id searchCopiedTextHandler = ^(UIAction* action) {
        [self searchCopiedText:nil];
      };
      UIAction* searchCopiedTextAction = [UIAction
          actionWithTitle:l10n_util::GetNSString((IDS_IOS_SEARCH_COPIED_TEXT))
                    image:pasteImage
               identifier:nil
                  handler:searchCopiedTextHandler];
      [menuElements addObject:searchCopiedTextAction];
    }
  }

  // Show Top or Bottom Address Bar action.
  if (IsBottomOmniboxAvailable() && IsSplitToolbarMode(self)) {
    NSString* title = nil;
    UIImage* image = nil;
    ToolbarType targetToolbarType;
    if (GetApplicationContext()->GetLocalState()->GetBoolean(
            prefs::kBottomOmnibox)) {
      title = l10n_util::GetNSString(IDS_IOS_TOOLBAR_MENU_TOP_OMNIBOX);
      if (@available(iOS 15.1, *)) {
        image = DefaultSymbolWithPointSize(kMovePlatterToTopPhoneSymbol,
                                           kSymbolActionPointSize);
      } else {
        image = CustomSymbolWithPointSize(kCustomMovePlatterToTopPhoneSymbol,
                                          kSymbolActionPointSize);
      }
      targetToolbarType = ToolbarType::kPrimary;
    } else {
      title = l10n_util::GetNSString(IDS_IOS_TOOLBAR_MENU_BOTTOM_OMNIBOX);
      if (@available(iOS 15.1, *)) {
        image = DefaultSymbolWithPointSize(kMovePlatterToBottomPhoneSymbol,
                                           kSymbolActionPointSize);
      } else {
        image = CustomSymbolWithPointSize(kCustomMovePlatterToBottomPhoneSymbol,
                                          kSymbolActionPointSize);
      }
      targetToolbarType = ToolbarType::kSecondary;
    }
    UIAction* moveAddressBarAction = [UIAction
        actionWithTitle:title
                  image:image
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf moveOmniboxToToolbarType:targetToolbarType];
                }];

    UIMenu* divider = [UIMenu menuWithTitle:@""
                                      image:nil
                                 identifier:nil
                                    options:UIMenuOptionsDisplayInline
                                   children:@[ moveAddressBarAction ]];
    [menuElements addObject:divider];
  }

  return [UIMenu menuWithTitle:@"" children:menuElements];
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
    previewForHighlightingMenuWithConfiguration:
        (UIContextMenuConfiguration*)configuration {
  // Use the location bar's container view because that's the view that has the
  // background color and corner radius.
  return [[UITargetedPreview alloc]
      initWithView:self.view.superview
        parameters:[[UIPreviewParameters alloc] init]];
}

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  __weak LocationBarViewController* weakSelf = self;

  UIContextMenuConfiguration* configuration = [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu*(
                       NSArray<UIMenuElement*>* suggestedActions) {
                     return [weakSelf contextMenuUIMenu:suggestedActions];
                   }];

  if (IsBottomOmniboxAvailable()) {
    configuration.preferredMenuElementOrder =
        UIContextMenuConfigurationElementOrderPriority;
  }
  return configuration;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // Allow copying if the steady location bar is visible.
  if (!self.locationBarSteadyView.hidden && action == @selector(copy:)) {
    return YES;
  }

  return NO;
}

- (void)copy:(id)sender {
  [self.delegate locationBarCopyTapped];
}

- (void)searchCopiedImage:(id)sender {
  RecordAction(
      UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedImage"));
  [self.delegate searchCopiedImage];
}

- (void)lensCopiedImage:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.LensCopiedImage"));
  [self.delegate lensCopiedImage];
}

- (void)visitCopiedLink:(id)sender {
  // A search using clipboard link is activity that should indicate a user
  // that would be interested in setting Chrome as the default browser.
  [self.delegate locationBarVisitCopyLinkTapped];
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.VisitCopiedLink"));
  ClipboardRecentContent::GetInstance()->GetRecentURLFromClipboard(
      base::BindOnce(^(std::optional<GURL> optionalURL) {
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
  [self.delegate locationBarSearchCopiedTextTapped];
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedText"));
  ClipboardRecentContent::GetInstance()->GetRecentTextFromClipboard(
      base::BindOnce(^(std::optional<std::u16string> optionalText) {
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

/// Set the preferred omnibox position to `toolbarType`.
- (void)moveOmniboxToToolbarType:(ToolbarType)toolbarType {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kBottomOmnibox, toolbarType == ToolbarType::kSecondary);

  if (toolbarType == ToolbarType::kPrimary) {
    RecordAction(
        UserMetricsAction("Mobile.OmniboxContextMenu.MoveAddressBarToTop"));
  } else {
    RecordAction(
        UserMetricsAction("Mobile.OmniboxContextMenu.MoveAddressBarToBottom"));
  }
}

// Creates and shows the lens overlay UI.
- (void)openLensOverlay {
  if (self.tracker) {
    self.tracker->NotifyEvent(
        feature_engagement::events::kLensOverlayEntrypointUsed);
  }
  RecordAction(UserMetricsAction("MobileToolbarLensOverlayTap"));
  TriggerHapticFeedbackForSelectionChange();
  [self.dispatcher createAndShowLensUI:YES
                            entrypoint:LensOverlayEntrypoint::kLocationBar
                            completion:nil];
}

- (void)updatePlaceholderView {
  switch (_placeholderType) {
    case LocationBarPlaceholderType::kNone:
      self.locationBarSteadyView.placeholderView = nil;
      break;
    case LocationBarPlaceholderType::kLensOverlay:
      self.locationBarSteadyView.placeholderView = _lensOverlayPlaceholderView;
      break;
  }
}

@end
