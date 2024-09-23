// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_command_handler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

enum SectionIdentifier {
  kSectionIdentifierLinkouts = kSectionIdentifierEnumZero
};

enum ItemType {
  kItemTypeHeader = kItemTypeEnumZero,
  kItemTypeWebAndAppActivity,
  kItemTypeLinkedGoogleServices,
};

}  // namespace

@implementation PersonalizeGoogleServicesViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(
      IDS_IOS_MANAGE_SYNC_PERSONALIZE_GOOGLE_SERVICES_TITLE_EEA);
  self.view.accessibilityIdentifier = kPersonalizeGoogleServicesViewIdentifier;

  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        personalizeGoogleServicesViewcontrollerDidRemove:self];
  }
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  // Linkouts section.
  [model addSectionWithIdentifier:kSectionIdentifierLinkouts];

  // Header item.
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:kItemTypeHeader];
  headerItem.text = l10n_util::GetNSStringF(
      IDS_IOS_PERSONALIZE_GOOGLE_SERVICES_HEADER,
      l10n_util::GetStringUTF16(IDS_IOS_PERSONALIZE_GOOGLE_SERVICES_WAA_TITLE),
      l10n_util::GetStringUTF16(
          IDS_IOS_PERSONALIZE_GOOGLE_SERVICES_LINKED_SERVICES_TITLE));
  [model setHeader:headerItem
      forSectionWithIdentifier:kSectionIdentifierLinkouts];

  // Web and App Activity item.
  TableViewImageItem* webAndAppActivityItem =
      [[TableViewImageItem alloc] initWithType:kItemTypeWebAndAppActivity];
  webAndAppActivityItem.accessoryView = [[UIImageView alloc]
      initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                        kExternalLinkSymbol)];
  webAndAppActivityItem.accessoryView.tintColor =
      [UIColor colorNamed:kTextQuaternaryColor];
  webAndAppActivityItem.title =
      l10n_util::GetNSString(IDS_IOS_PERSONALIZE_GOOGLE_SERVICES_WAA_TITLE);
  webAndAppActivityItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:webAndAppActivityItem
      toSectionWithIdentifier:kSectionIdentifierLinkouts];

  // Linked Google Services item.
  TableViewImageItem* linkedGoogleServicesItem =
      [[TableViewImageItem alloc] initWithType:kItemTypeLinkedGoogleServices];
  linkedGoogleServicesItem.accessoryView = [[UIImageView alloc]
      initWithImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                        kExternalLinkSymbol)];
  linkedGoogleServicesItem.accessoryView.tintColor =
      [UIColor colorNamed:kTextQuaternaryColor];
  linkedGoogleServicesItem.title = l10n_util::GetNSString(
      IDS_IOS_PERSONALIZE_GOOGLE_SERVICES_LINKED_SERVICES_TITLE);
  linkedGoogleServicesItem.detailText = l10n_util::GetNSString(
      IDS_IOS_PERSONALIZE_GOOGLE_SERVICES_LINKED_SERVICES_DESCRIPTION);
  linkedGoogleServicesItem.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:linkedGoogleServicesItem
      toSectionWithIdentifier:kSectionIdentifierLinkouts];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case kItemTypeWebAndAppActivity:
      [self.handler openWebAppActivityDialog];
      break;
    case kItemTypeLinkedGoogleServices:
      [self.handler openLinkedGoogleServicesDialog];
      break;
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

@end
