// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ItemArray = NSArray<TableViewItem*>*;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSafeBrowsingEnhancedProtection = kSectionIdentifierEnumZero,
  SectionIdentifierWhenOn,
  SectionIdentifierThingsToConsider,
};

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeShieldIcon = kItemTypeEnumZero,
  ItemTypeGIcon,
  ItemTypeGlobeIcon,
  ItemTypeKeyIcon,
  ItemTypeAccountIcon,
  ItemTypeMetricIcon,
  ItemTypeDataIcon,
  ItemTypeDownloadIcon,
  ItemTypeLinkIcon,
  ItemTypePerformanceIcon,
  ItemTypeEnhancedProtectionFirstHeader,
  ItemTypeEnhancedProtectionSecondHeader,
  ItemTypeEnhancedProtectionFooter,
};

// The size of the symbols.
const CGFloat kSymbolSize = 20;

}  // namespace

@interface SafeBrowsingEnhancedProtectionViewController ()

// All items on the enhance safe browsing settings menu.
@property(nonatomic, strong) ItemArray firstSectionItems;
@property(nonatomic, strong) ItemArray secondSectionItems;

// Footer item.
@property(nonatomic, strong)
    TableViewHeaderFooterItem* safeBrowsingEnhancedProtectionFooterItem;

@end

@implementation SafeBrowsingEnhancedProtectionViewController

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    // Wraps view controller to properly show navigation bar, otherwise "Done"
    // button won't show.
    self.navigationController =
        [[UINavigationController alloc] initWithRootViewController:self];
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(dismiss)];
    self.navigationController.modalPresentationStyle =
        UIModalPresentationFormSheet;
    self.navigationItem.rightBarButtonItem = doneButton;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kSafeBrowsingEnhancedProtectionTableViewId;
  self.tableView.separatorColor = UIColor.clearColor;
  self.title =
      l10n_util::GetNSString(IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE);
  self.styler.cellBackgroundColor = UIColor.clearColor;
  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.presentationDelegate
      safeBrowsingEnhancedProtectionViewControllerDidRemove:self];
  [super viewDidDisappear:animated];
}

#pragma mark - Private

// Removes the view as a result of pressing "Done" button.
- (void)dismiss {
  [self dismissViewControllerAnimated:YES completion:nil];
}

// Creates item that will show what Enhanced Protection entails.
- (SettingsImageDetailTextItem*)detailItemWithType:(NSInteger)type
                                        detailText:(NSInteger)detailText
                                             image:(UIImage*)image
                           accessibilityIdentifier:
                               (NSString*)accessibilityIdentifier {
  SettingsImageDetailTextItem* detailItem =
      [[SettingsImageDetailTextItem alloc] initWithType:type];
  detailItem.detailText = l10n_util::GetNSString(detailText);
  detailItem.alignImageWithFirstLineOfText = YES;
  detailItem.image = image;
  detailItem.imageViewTintColor = [UIColor colorNamed:kGrey600Color];
  detailItem.accessibilityIdentifier = accessibilityIdentifier;

  return detailItem;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction(
      "MobileSafeBrowsingEnhancedProtectionSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction(
      "MobileSafeBrowsingEnhancedProtectionSettingsBack"));
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
    [model addSectionWithIdentifier:SectionIdentifierWhenOn];
    [model setHeader:[self showFirstHeader]
        forSectionWithIdentifier:SectionIdentifierWhenOn];

    for (TableViewItem* item in self.firstSectionItems) {
      [model addItem:item toSectionWithIdentifier:SectionIdentifierWhenOn];
    }
    [model addSectionWithIdentifier:SectionIdentifierThingsToConsider];
    [model setHeader:[self showSecondHeader]
        forSectionWithIdentifier:SectionIdentifierThingsToConsider];

    for (TableViewItem* item in self.secondSectionItems) {
      [model addItem:item
          toSectionWithIdentifier:SectionIdentifierThingsToConsider];
    }

    [model setFooter:self.safeBrowsingEnhancedProtectionFooterItem
        forSectionWithIdentifier:SectionIdentifierThingsToConsider];
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        safeBrowsingEnhancedProtectionViewControllerDidRemove:self];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction(
      "IOSSafeBrowsingEnhancedProtectionSettingsCloseWithSwipe"));
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  if (sectionIdentifier == SectionIdentifierThingsToConsider) {
    // Might be a different type of footer.
    TableViewLinkHeaderFooterView* linkView =
        base::apple::ObjCCast<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }
  return view;
}

#pragma mark - UITableViewDelegate

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  // None of the items in this page should be allowed to be highlighted. This
  // also removes the ability to select a row since highlighting comes before
  // selecting a row.
  return NO;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  [self.applicationHandler closeSettingsUIAndOpenURL:command];
}

#pragma mark - Properties

- (ItemArray)firstSectionItems {
  if (!_firstSectionItems) {
    NSMutableArray* items = [NSMutableArray array];

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    UIImage* gIcon =
        CustomSymbolWithPointSize(kGoogleShieldSymbol, kSymbolSize);
#else
    UIImage* gIcon = DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolSize);
#endif

    NSInteger gIconDetailText =
        IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_G_ICON_DESCRIPTION;
    SettingsImageDetailTextItem* gIconItem =
        [self detailItemWithType:ItemTypeGIcon
                         detailText:gIconDetailText
                              image:gIcon
            accessibilityIdentifier:kSafeBrowsingEnhancedProtectionGIconCellId];

    UIImage* globeIcon =
        DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kSymbolSize);
    SettingsImageDetailTextItem* globeIconItem = [self
             detailItemWithType:ItemTypeGlobeIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_GLOBE_ICON_DESCRIPTION
                          image:globeIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionGlobeCellId];

    NSInteger keyIconDetailText =
        IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_KEY_ICON_DESCRIPTION;
    UIImage* keyIcon = CustomSymbolWithPointSize(kPasswordSymbol, kSymbolSize);
    SettingsImageDetailTextItem* keyIconItem =
        [self detailItemWithType:ItemTypeKeyIcon
                         detailText:keyIconDetailText
                              image:keyIcon
            accessibilityIdentifier:kSafeBrowsingEnhancedProtectionKeyCellId];
      UIImage* dataIcon =
          DefaultSymbolWithPointSize(kChartBarXAxisSymbol, kSymbolSize);
      SettingsImageDetailTextItem* dataIconItem = [self
               detailItemWithType:ItemTypeDataIcon
                       detailText:
                           IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_DATA_ICON_DESCRIPTION
                            image:dataIcon
          accessibilityIdentifier:kSafeBrowsingEnhancedProtectionDataCellId];

      UIImage* downloadIcon =
          DefaultSymbolWithPointSize(kSaveImageActionSymbol, kSymbolSize);
      SettingsImageDetailTextItem* downloadIconItem = [self
               detailItemWithType:ItemTypeDownloadIcon
                       detailText:
                           IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_DOWNLOAD_ICON_DESCRIPTION
                            image:downloadIcon
          accessibilityIdentifier:
              kSafeBrowsingEnhancedProtectionDownloadCellId];

      [items addObject:dataIconItem];
      [items addObject:downloadIconItem];
      [items addObject:gIconItem];
      [items addObject:globeIconItem];
      [items addObject:keyIconItem];

    _firstSectionItems = items;
  }
  return _firstSectionItems;
}

- (ItemArray)secondSectionItems {
  if (!_secondSectionItems) {
    NSMutableArray* items = [NSMutableArray array];

    UIImage* linkIcon =
        DefaultSymbolWithPointSize(kLinkActionSymbol, kSymbolSize);
    SettingsImageDetailTextItem* linkIconItem = [self
             detailItemWithType:ItemTypeLinkIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_LINK_ICON_DESCRIPTION
                          image:linkIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionLinkCellId];

    UIImage* accountIcon =
        DefaultSymbolWithPointSize(kPersonCropCircleSymbol, kSymbolSize);
    SettingsImageDetailTextItem* accountIconItem = [self
             detailItemWithType:ItemTypeAccountIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_ACCOUNT_ICON_DESCRIPTION
                          image:accountIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionAccountCellId];

    UIImage* performanceIcon =
        DefaultSymbolWithPointSize(kSpeedometerSymbol, kSymbolSize);
    SettingsImageDetailTextItem* performanceIconItem = [self
             detailItemWithType:ItemTypePerformanceIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_PERFORMANCE_ICON_DESCRIPTION
                          image:performanceIcon
        accessibilityIdentifier:
            kSafeBrowsingEnhancedProtectionPerformanceCellId];

    [items addObject:linkIconItem];
    [items addObject:accountIconItem];
    [items addObject:performanceIconItem];
    _secondSectionItems = items;
  }

  return _secondSectionItems;
}

- (TableViewHeaderFooterItem*)safeBrowsingEnhancedProtectionFooterItem {
  if (!_safeBrowsingEnhancedProtectionFooterItem) {
    TableViewLinkHeaderFooterItem* enhancedProtectionFooterItem =
        [[TableViewLinkHeaderFooterItem alloc]
            initWithType:ItemTypeEnhancedProtectionFooter];

    NSMutableArray* urls = [[NSMutableArray alloc] init];
    NSString* enhancedSafeBrowsingFooterText = l10n_util::GetNSString(
        IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_FOOTER);
    [urls addObject:[[CrURL alloc]
                        initWithGURL:GURL(kEnhancedSafeBrowsingLearnMoreURL)]];

    enhancedProtectionFooterItem.text = enhancedSafeBrowsingFooterText;
    enhancedProtectionFooterItem.urls = urls;
    enhancedProtectionFooterItem.accessibilityIdentifier =
        kSafeBrowsingEnhancedProtectionTableViewFooterId;
    enhancedProtectionFooterItem.forceIndents = YES;
    _safeBrowsingEnhancedProtectionFooterItem = enhancedProtectionFooterItem;
  }

  return _safeBrowsingEnhancedProtectionFooterItem;
}

- (TableViewHeaderFooterItem*)showFirstHeader {
  TableViewTextHeaderFooterItem* firstHeaderItem =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:ItemTypeEnhancedProtectionFirstHeader];
  firstHeaderItem.text = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_WHEN_ON_HEADER);
  firstHeaderItem.forceIndents = YES;
  firstHeaderItem.accessibilityIdentifier =
      kSafeBrowsingEnhancedProtectionTableViewFirstHeaderId;

  return firstHeaderItem;
}

- (TableViewHeaderFooterItem*)showSecondHeader {
  TableViewTextHeaderFooterItem* secondHeaderItem =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:ItemTypeEnhancedProtectionSecondHeader];
  secondHeaderItem.text = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_THINGS_TO_CONSIDER_HEADER);
  secondHeaderItem.forceIndents = YES;
  secondHeaderItem.accessibilityIdentifier =
      kSafeBrowsingEnhancedProtectionTableViewSecondHeaderId;

  return secondHeaderItem;
}
@end
