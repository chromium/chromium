// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/remote_actor/remote_actor_config.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_mutator.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

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

// Returns the display name of the remote actor.
std::u16string GetActorName() {
  return l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME);
}

}  // namespace

@interface RemoteActorCredentialBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UITableViewDataSource,
    UITextViewDelegate>
@end

@implementation RemoteActorCredentialBottomSheetViewController {
  // Table view displaying the credential suggestions.
  UITableView* _tableView;

  // List of credential suggestions to display.
  NSArray<FormSuggestion*>* _suggestions;

  // Favicon attributes for the domain.
  FaviconAttributes* _faviconAttributes;

  // Range of the link in `subtitleString`.
  NSRange _subtitleLinkRange;

  // URL opened when the link in the subtitle is tapped.
  GURL _learnMoreURL;

  // Domain requesting the credentials.
  NSString* _domain;

  // Whether the sheet shows that there are no saved passwords for `_domain`.
  BOOL _noPasswordsFound;

  // Subtitle text view, kept to update the subtitle after the view has loaded.
  __weak UITextView* _subtitleTextView;
}

- (instancetype)init {
  self = [super initWithConfiguration:SetUpButtonConfiguration()];
  if (self) {
    _learnMoreURL = password_manager::GetRemoteActorConfig(
                        password_manager::RemoteActorType::kGemini)
                        .learn_more_url;
    self.actionHandler = self;
  }
  return self;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // There is nothing to share, so going back to the remote actor ends the flow.
  if (_noPasswordsFound) {
    [self.mutator didCancel];
    return;
  }
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

  if (_noPasswordsFound) {
    // Set the subtitle string if the empty state was triggered before load.
    self.subtitleString = l10n_util::GetNSStringF(
        IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_SUBTITLE, GetActorName());
  } else {
    StringWithTags subtitle = ParseStringWithLinks(
        l10n_util::GetNSStringF(IDS_REMOTE_ACTOR_SHARING_DIALOG_SUBTITLE,
                                kBeginLinkTag, kEndLinkTag, GetActorName()));
    self.subtitleString = subtitle.string;
    _subtitleLinkRange = subtitle.ranges[0];
  }

  [super viewDidLoad];

  // Styles the no saved passwords subtitle the same way as when the empty
  // state is shown after the view loaded.
  if (_noPasswordsFound) {
    [self replaceLoadedSubtitleText];
  }

  [self adjustTransactionsButtonHorizontalConstraints];
}

#pragma mark - RemoteActorCredentialBottomSheetConsumer

- (void)setDomain:(NSString*)domain {
  _domain = [domain copy];
  [self updateTitle];
}

- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  if (suggestions.count > 0) {
    _suggestions = [suggestions copy];
    self.configuration.primaryActionEnabled = YES;
    [self reloadConfiguration];
  }

  // The suggestions may arrive after the view has loaded.
  if (self.viewLoaded) {
    [self reloadTableViewData];
  }
}

- (void)setFaviconAttributes:(FaviconAttributes*)faviconAttributes {
  _faviconAttributes = faviconAttributes;
  // The favicon is fetched asynchronously, so it may arrive after the cells
  // have been laid out.
  for (UITableViewCell* cell in _tableView.visibleCells) {
    TableViewCellContentConfiguration* configuration =
        base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
            cell.contentConfiguration);
    configuration.leadingConfiguration =
        [self faviconConfiguration:faviconAttributes];
    cell.contentConfiguration = configuration;
  }
}

- (void)showNoSavedPasswords {
  _noPasswordsFound = YES;
  [self updateTitle];

  self.subtitleString = l10n_util::GetNSStringF(
      IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_SUBTITLE, GetActorName());
  [self replaceLoadedSubtitleText];

  ButtonStackConfiguration* configuration = self.configuration;
  configuration.primaryButtonStyle = ChromeButtonStylePrimary;
  configuration.primaryActionString = l10n_util::GetNSStringF(
      IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_BACK_BUTTON, GetActorName());
  configuration.primaryActionEnabled = YES;
  configuration.secondaryActionString = nil;
  [self reloadConfiguration];

  // VoiceOver would otherwise keep reading the previous content.
  if (self.viewLoaded) {
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    self.titleLabel);
  }
}

#pragma mark - ButtonStackConsumer

- (void)setLoading:(BOOL)loading {
  // Nothing can be shared without saved passwords, so there is no loading
  // state.
  CHECK(!_noPasswordsFound);
  self.configuration.primaryButtonStyle =
      loading ? ChromeButtonStylePrimary : ChromeButtonStyleTertiary;
  [super setLoading:loading];
  [self reloadConfiguration];

  // The spinner replaces the button title, so VoiceOver needs a label.
  self.primaryActionButton.accessibilityLabel =
      loading ? l10n_util::GetNSString(
                    IDS_IOS_PASSWORD_SHARING_STATUS_PROGRESS_TITLE)
              : nil;
}

- (void)reloadConfiguration {
  [super reloadConfiguration];
  // Keep the cancel button enabled while the selected credential is shared.
  self.secondaryActionButton.enabled = YES;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [self rowCount];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  return [self layoutCell:cell atIndexPath:indexPath];
}

#pragma mark - TableViewBottomSheetViewController

- (UITableView*)createTableView {
  _tableView = [super createTableView];
  _tableView.dataSource = self;
  [TableViewCellContentConfiguration registerCellForTableView:_tableView];
  return _tableView;
}

- (NSUInteger)rowCount {
  return _suggestions.count;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  CGFloat tableWidth = [self tableViewWidth];
  UITableViewCell* cell = [self layoutCell:[[UITableViewCell alloc] init]
                               atIndexPath:[NSIndexPath indexPathForRow:index
                                                              inSection:0]];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(tableWidth, 1)].height;
}

- (BOOL)shouldAlwaysUseLargeDetent {
  return YES;
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  [super customizeSubtitle:subtitle];
  _subtitleTextView = subtitle;
  // The no saved passwords subtitle has no link.
  if (_noPasswordsFound) {
    return;
  }
  subtitle.delegate = self;
  subtitle.selectable = YES;
  NSMutableAttributedString* newSubtitle = [[NSMutableAttributedString alloc]
      initWithAttributedString:subtitle.attributedText];
  NSDictionary* linkAttributes = @{
    NSLinkAttributeName : net::NSURLWithGURL(_learnMoreURL),
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle),
  };
  [newSubtitle addAttributes:linkAttributes range:_subtitleLinkRange];
  subtitle.attributedText = newSubtitle;
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  __weak __typeof(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf openLearnMoreURL];
  }];
}

- (UITextItemMenuConfiguration*)textView:(UITextView*)textView
            menuConfigurationForTextItem:(UITextItem*)textItem
                             defaultMenu:(UIMenu*)defaultMenu {
  // Disable the system context menu so the link always opens inside Chrome.
  return nil;
}

#pragma mark - Private

// Sets the title for `_domain` matching the current state, updating the title
// label if the view has already loaded.
- (void)updateTitle {
  std::u16string domain = base::SysNSStringToUTF16(_domain);
  self.titleString =
      _noPasswordsFound
          ? l10n_util::GetNSStringF(
                IDS_IOS_REMOTE_ACTOR_NO_PASSWORD_DIALOG_TITLE, domain)
          : l10n_util::GetNSStringF(IDS_REMOTE_ACTOR_SHARING_DIALOG_TITLE,
                                    GetActorName(), domain);
  self.titleLabel.text = self.titleString;
}

// Replaces the loaded subtitle with `subtitleString`. The text is rebuilt with
// explicit attributes so none of the link styling carries over.
- (void)replaceLoadedSubtitleText {
  if (!_subtitleTextView) {
    return;
  }
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = _subtitleTextView.textAlignment;
  NSDictionary* attributes = @{
    NSFontAttributeName : _subtitleTextView.font
        ?: [UIFont preferredFontForTextStyle:self.subtitleTextStyle],
    NSForegroundColorAttributeName : _subtitleTextView.textColor
        ?: [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : paragraphStyle,
  };
  _subtitleTextView.attributedText =
      [[NSAttributedString alloc] initWithString:self.subtitleString
                                      attributes:attributes];
  _subtitleTextView.selectable = NO;
  _subtitleTextView.delegate = nil;
}

// Opens `_learnMoreURL` in a new tab, closing the presented views first.
- (void)openLearnMoreURL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:_learnMoreURL];
  [self.sceneHandler closePresentedViewsAndOpenURL:command];
}

// Returns the leading favicon configuration for `faviconAttributes`, or nil if
// the favicon isn't available yet.
- (FaviconContentConfiguration*)faviconConfiguration:
    (FaviconAttributes*)faviconAttributes {
  if (!faviconAttributes) {
    return nil;
  }
  FaviconContentConfiguration* configuration =
      [[FaviconContentConfiguration alloc] init];
  configuration.faviconAttributes = faviconAttributes;
  return configuration;
}

// Returns the accessibility value for the cell at `indexPath`.
- (NSString*)cellAccessibilityValueAtIndexPath:(NSIndexPath*)indexPath {
  return l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE,
                                 base::NumberToString16(indexPath.row + 1),
                                 base::NumberToString16([self rowCount]));
}

// Lays out `cell` for the credential suggestion at `indexPath`.
- (UITableViewCell*)layoutCell:(UITableViewCell*)cell
                   atIndexPath:(NSIndexPath*)indexPath {
  CHECK_LT(static_cast<NSUInteger>(indexPath.row), _suggestions.count);
  NSString* username = _suggestions[indexPath.row].value;

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title =
      username.length
          ? username
          : l10n_util::GetNSString(IDS_IOS_CREDENTIAL_BOTTOM_SHEET_NO_USERNAME);
  configuration.titleNumberOfLines = 1;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingMiddle;
  configuration.subtitle = l10n_util::GetNSString(IDS_IOS_PASSWORD_SUBTEXT);
  configuration.subtitleNumberOfLines = 1;
  configuration.leadingConfiguration =
      [self faviconConfiguration:_faviconAttributes];

  cell.contentConfiguration = configuration;
  cell.accessibilityIdentifier = configuration.title;
  cell.accessibilityValue = [self cellAccessibilityValueAtIndexPath:indexPath];
  cell.accessoryType = [self accessoryType:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  return cell;
}

@end
