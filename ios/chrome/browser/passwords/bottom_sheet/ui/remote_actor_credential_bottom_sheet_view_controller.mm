// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_mutator.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Link delimiters expected by `ParseStringWithLinks()`.
constexpr char16_t kBeginLinkTag[] = u"BEGIN_LINK";
constexpr char16_t kEndLinkTag[] = u"END_LINK";

// Point size of the icons displayed in the header badges.
constexpr CGFloat kHeaderIconPointSize = 30.0;

// Creates a rounded square badge view containing `image`.
UIView* CreateIconBadgeView(UIImage* image) {
  static constexpr CGFloat kHeaderBadgeSize = 60.0;
  static constexpr CGFloat kHeaderBadgeCornerRadius = 15.0;
  static constexpr CGFloat kHeaderBadgeShadowRadius = 6.0;
  static constexpr CGFloat kHeaderBadgeShadowOffsetY = 2.0;
  static constexpr float kHeaderBadgeShadowOpacity = 0.12f;

  UIView* badge_view = [[UIView alloc] init];
  badge_view.translatesAutoresizingMaskIntoConstraints = NO;
  badge_view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  badge_view.layer.cornerRadius = kHeaderBadgeCornerRadius;
  badge_view.layer.shadowOffset = CGSizeMake(0, kHeaderBadgeShadowOffsetY);
  badge_view.layer.shadowRadius = kHeaderBadgeShadowRadius;
  badge_view.layer.shadowColor = [UIColor colorNamed:kSolidBlackColor].CGColor;
  badge_view.layer.shadowOpacity = kHeaderBadgeShadowOpacity;
  [badge_view
      registerForTraitChanges:@[ [UITraitUserInterfaceStyle class] ]
                  withHandler:^(UIView* view, UITraitCollection* previous) {
                    view.layer.shadowColor =
                        [UIColor colorNamed:kSolidBlackColor].CGColor;
                  }];

  UIImageView* image_view = [[UIImageView alloc] initWithImage:image];
  image_view.translatesAutoresizingMaskIntoConstraints = NO;
  [badge_view addSubview:image_view];

  AddSquareConstraints(badge_view, kHeaderBadgeSize);
  AddSameCenterConstraints(image_view, badge_view);

  return badge_view;
}

// Creates the header illustration above the title showing the Gemini badge and
// the Google Password Manager key badge.
UIView* CreateAboveTitleView() {
  static constexpr CGFloat kHeaderBadgeContainerWidth = 105.0;
  static constexpr CGFloat kHeaderBadgeContainerHeight = 95.0;

  UIView* container_view = [[UIView alloc] init];
  container_view.translatesAutoresizingMaskIntoConstraints = NO;
  // The header is decorative; the title below provides the context.
  container_view.accessibilityElementsHidden = YES;

  UIView* gemini_badge = CreateIconBadgeView(
      [GeminiUIUtils createGradientGeminiLogo:kHeaderIconPointSize]);
  UIImage* password_manager_icon = MakeSymbolMulticolor(
      SymbolWithPointSize(SymbolPasswordManager, kHeaderIconPointSize));
  UIView* password_manager_badge = CreateIconBadgeView(password_manager_icon);

  [container_view addSubview:gemini_badge];
  [container_view addSubview:password_manager_badge];

  AddSizeConstraints(container_view, CGSizeMake(kHeaderBadgeContainerWidth,
                                                kHeaderBadgeContainerHeight));
  AddSameConstraintsToSides(gemini_badge, container_view,
                            LayoutSides::kTop | LayoutSides::kLeading);
  AddSameConstraintsToSides(password_manager_badge, container_view,
                            LayoutSides::kBottom | LayoutSides::kTrailing);

  return container_view;
}

// Creates the default button stack configuration using the tertiary style so
// both options are equally weighted.
ButtonStackConfiguration* SetUpButtonConfiguration() {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryButtonStyle = ChromeButtonStyleTertiary;
  configuration.secondaryButtonStyle = ChromeButtonStyleTertiary;
  configuration.primaryActionString =
      l10n_util::GetNSString(IDS_REMOTE_ACTOR_SHARING_DIALOG_ALLOW_BUTTON);
  configuration.secondaryActionString = l10n_util::GetNSString(IDS_CANCEL);
  // Disabled until there are suggestions to share.
  configuration.primaryActionEnabled = NO;
  return configuration;
}

}  // namespace

@interface RemoteActorCredentialBottomSheetViewController () <
    ConfirmationAlertActionHandler>
@end

@implementation RemoteActorCredentialBottomSheetViewController {
  // List of credential suggestions to display.
  NSArray<FormSuggestion*>* _suggestions;

  // Favicon attributes for the domain.
  FaviconAttributes* _faviconAttributes;

  // Range of the link in `subtitleString`.
  NSRange _subtitleLinkRange;
}

- (instancetype)init {
  self = [super initWithConfiguration:SetUpButtonConfiguration()];
  if (self) {
    self.actionHandler = self;
  }
  return self;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.mutator didSelectCredentialAtIndex:[self selectedRow]];
}

- (void)confirmationAlertSecondaryAction {
  [self.mutator didCancel];
}

- (void)confirmationAlertDismissed {
  [self.mutator didCancel];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  static constexpr CGFloat kCustomSpacingBeforeImage = 16.0;
  static constexpr CGFloat kCustomSpacing = 12.0;

  self.aboveTitleView = CreateAboveTitleView();
  self.customSpacingBeforeImage = kCustomSpacingBeforeImage;
  self.customSpacing = kCustomSpacing;

  self.titleTextStyle = UIFontTextStyleLargeTitle;
  self.subtitleTextStyle = UIFontTextStyleSubheadline;

  StringWithTags subtitle = ParseStringWithLinks(
      l10n_util::GetNSStringF(IDS_REMOTE_ACTOR_SHARING_DIALOG_SUBTITLE,
                              kBeginLinkTag, kEndLinkTag, u"Gemini"));
  self.subtitleString = subtitle.string;
  _subtitleLinkRange = subtitle.ranges[0];

  [super viewDidLoad];

  [self adjustTransactionsButtonHorizontalConstraints];
}

#pragma mark - RemoteActorCredentialBottomSheetConsumer

- (void)setDomain:(NSString*)domain {
  self.titleString =
      l10n_util::GetNSStringF(IDS_REMOTE_ACTOR_SHARING_DIALOG_TITLE, u"Gemini",
                              base::SysNSStringToUTF16(domain));
}

- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  if (suggestions.count > 0) {
    _suggestions = [suggestions copy];
    self.configuration.primaryActionEnabled = YES;
    [self reloadConfiguration];
  }
}

- (void)setFaviconAttributes:(FaviconAttributes*)faviconAttributes {
  _faviconAttributes = faviconAttributes;
}

#pragma mark - TableViewBottomSheetViewController

- (NSUInteger)rowCount {
  // TODO(crbug.com/579012416): Update `rowCount` to return the number of
  // credentials to display.
  return 0;
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  [super customizeSubtitle:subtitle];
  subtitle.selectable = YES;
  // TODO(crbug.com/579384541): Handle taps on the subtitle link.
  NSMutableAttributedString* newSubtitle = [[NSMutableAttributedString alloc]
      initWithAttributedString:subtitle.attributedText];
  NSDictionary* linkAttributes = @{
    NSLinkAttributeName : @"",
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle),
  };
  [newSubtitle addAttributes:linkAttributes range:_subtitleLinkRange];
  subtitle.attributedText = newSubtitle;
}

@end
