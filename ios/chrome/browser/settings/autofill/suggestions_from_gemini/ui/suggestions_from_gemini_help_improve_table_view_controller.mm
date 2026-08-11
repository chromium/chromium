// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_table_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface SuggestionsFromGeminiHelpImproveTableViewController () <
    PopoverLabelViewControllerDelegate>
@end

namespace {

typedef NS_ENUM(NSInteger, HelpImproveSectionIdentifier) {
  HelpImproveSectionIdentifierHelpingImprove = kSectionIdentifierEnumZero,
  HelpImproveSectionIdentifierWhenUsed,
  HelpImproveSectionIdentifierThingsToConsider,
};

typedef NS_ENUM(NSInteger, HelpImproveItemType) {
  HelpImproveItemTypeHeader = kItemTypeEnumZero,
  HelpImproveItemTypeFooter,
  HelpImproveItemTypeLabel,
  HelpImproveItemTypeDetailIcon,
};

// Creates and returns a configured TableViewTextHeaderFooterItem.
TableViewTextHeaderFooterItem* CreateHeaderItem(int messageId) {
  TableViewTextHeaderFooterItem* header = [[TableViewTextHeaderFooterItem alloc]
      initWithType:HelpImproveItemTypeHeader];
  header.text = l10n_util::GetNSString(messageId);
  return header;
}

// Creates and returns a configured TableViewDetailIconItem for the help
// improve subpage.
TableViewDetailIconItem* HelpImproveDetailItem(NSInteger titleId,
                                               Symbol symbol) {
  TableViewDetailIconItem* detailItem = [[TableViewDetailIconItem alloc]
      initWithType:HelpImproveItemTypeDetailIcon];
  detailItem.text = l10n_util::GetNSString(titleId);
  detailItem.textNumberOfLines = 0;
  detailItem.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  detailItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  detailItem.selectionStyle = UITableViewCellSelectionStyleNone;
  detailItem.iconImage =
      SymbolWithPointSize(symbol, kSettingsRootSymbolImagePointSize);
  detailItem.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  return detailItem;
}

}  // namespace

@implementation SuggestionsFromGeminiHelpImproveTableViewController {
  BOOL _settingsAreDismissed;
  // Tracks the enterprise policy state for Suggestions from Gemini.
  SuggestionsFromGeminiPolicyState _policyState;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE);
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  // Helping improve section.
  [model addSectionWithIdentifier:HelpImproveSectionIdentifierHelpingImprove];

  if (_policyState == SuggestionsFromGeminiPolicyState::kFullyAllowed) {
    TableViewTextItem* helpingImproveItem =
        [[TableViewTextItem alloc] initWithType:HelpImproveItemTypeLabel];
    helpingImproveItem.text = l10n_util::GetNSString(
        IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_TITLE);
    helpingImproveItem.selectionStyle = UITableViewCellSelectionStyleNone;
    [model addItem:helpingImproveItem
        toSectionWithIdentifier:HelpImproveSectionIdentifierHelpingImprove];
  } else {
    TableViewInfoButtonItem* helpingImproveItem =
        [[TableViewInfoButtonItem alloc] initWithType:HelpImproveItemTypeLabel];
    helpingImproveItem.text = l10n_util::GetNSString(
        IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE);
    helpingImproveItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    helpingImproveItem.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    helpingImproveItem.selectionStyle = UITableViewCellSelectionStyleNone;
    helpingImproveItem.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
    helpingImproveItem.target = self;
    helpingImproveItem.selector = @selector(didTapManagedUIInfoButton:);
    [model addItem:helpingImproveItem
        toSectionWithIdentifier:HelpImproveSectionIdentifierHelpingImprove];
  }

  TableViewTextHeaderFooterItem* helpingImproveFooter =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:HelpImproveItemTypeFooter];
  helpingImproveFooter.subtitle = l10n_util::GetNSString(
      IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_QUALITY_LOGGING_SUBTITLE);
  [model setFooter:helpingImproveFooter
      forSectionWithIdentifier:HelpImproveSectionIdentifierHelpingImprove];

  // When used section.
  [model addSectionWithIdentifier:HelpImproveSectionIdentifierWhenUsed];
  [model setHeader:
             CreateHeaderItem(
                 IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_WHEN_USED_TITLE)
      forSectionWithIdentifier:HelpImproveSectionIdentifierWhenUsed];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_1,
                     SymbolChartBarXAxis)
      toSectionWithIdentifier:HelpImproveSectionIdentifierWhenUsed];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_2,
                     SymbolMagnifyingglassSpark)
      toSectionWithIdentifier:HelpImproveSectionIdentifierWhenUsed];

  // Things to consider section.
  [model addSectionWithIdentifier:HelpImproveSectionIdentifierThingsToConsider];
  [model setHeader:
             CreateHeaderItem(
                 IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_THINGS_TO_CONSIDER_TITLE)
      forSectionWithIdentifier:HelpImproveSectionIdentifierThingsToConsider];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_1,
                     SymbolLinkAction)
      toSectionWithIdentifier:HelpImproveSectionIdentifierThingsToConsider];

  [model addItem:HelpImproveDetailItem(
                     IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_2,
                     SymbolPersonCropCircle)
      toSectionWithIdentifier:HelpImproveSectionIdentifierThingsToConsider];
  if (_policyState != SuggestionsFromGeminiPolicyState::kFullyAllowed) {
    [model addItem:HelpImproveDetailItem(
                       IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_3,
                       SymbolEnterprise)
        toSectionWithIdentifier:HelpImproveSectionIdentifierThingsToConsider];
  }
}

#pragma mark - Actions

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  if (_settingsAreDismissed) {
    return;
  }

  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:bubbleViewController animated:YES completion:nil];
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  if (_settingsAreDismissed) {
    return;
  }
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - SuggestionsFromGeminiHelpImproveConsumer

- (void)setSuggestionsFromGeminiPolicyState:
    (SuggestionsFromGeminiPolicyState)state {
  if (_policyState == state) {
    return;
  }

  _policyState = state;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("SuggestionsFromGeminiHelpImproveClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("SuggestionsFromGeminiHelpImproveBack"));
}

- (void)settingsWillBeDismissed {
  _settingsAreDismissed = YES;
}

@end
