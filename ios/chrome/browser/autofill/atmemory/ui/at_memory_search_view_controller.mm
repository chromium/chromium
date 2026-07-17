// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSearch = 0,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSearch = 0,
};

}  // namespace

@interface AtMemorySearchViewController () <
    TableViewLinkHeaderFooterItemDelegate>
@end

@implementation AtMemorySearchViewController {
  UITableViewDiffableDataSource<NSNumber*, AtMemorySearchItem*>* _dataSource;
  UIView* _disclaimerFooterView;
}

- (instancetype)initWithStyle:(UITableViewStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FIND_AND_FILL_TITLE);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelection = YES;
  [self setupDisclaimerFooter];
  [self loadModel];
}

- (void)setQuery:(NSString*)query {
  _query = [query copy];
  if (self.isViewLoaded) {
    [self loadModel];
  }
}

- (void)setLoading:(BOOL)loading {
  _loading = loading;
  if (self.isViewLoaded) {
    _disclaimerFooterView.hidden = loading;
    [self loadModel];
  }
}

- (void)setupDisclaimerFooter {
  TableViewLinkHeaderFooterView* footer = [[TableViewLinkHeaderFooterView alloc]
      initWithFrame:CGRectMake(0, 0, 320, 80)];
  footer.urls =
      @[ [[CrURL alloc] initWithGURL:autofill::GetManageYourInfoURL()] ];
  [footer setText:@"Gemini is AI and can make mistakes. BEGIN_LINKMore about "
                  @"relevant data sent to GoogleEND_LINK"
          withColor:[UIColor colorNamed:kTextSecondaryColor]
      textAlignment:NSTextAlignmentCenter
               font:[UIFont preferredFontForTextStyle:UIFontTextStyleCaption2]];
  footer.delegate = self;
  _disclaimerFooterView = footer;
  self.tableView.tableFooterView = _disclaimerFooterView;
}

- (void)loadModel {
  if (!_dataSource) {
    _dataSource = [[UITableViewDiffableDataSource alloc]
        initWithTableView:self.tableView
             cellProvider:^UITableViewCell*(UITableView* tableView,
                                            NSIndexPath* indexPath,
                                            AtMemorySearchItem* item) {
               [TableViewCellContentConfiguration
                   registerCellForTableView:tableView];
               UITableViewCell* cell = [TableViewCellContentConfiguration
                   dequeueTableViewCell:tableView
                           forIndexPath:indexPath];

               TableViewCellContentConfiguration* contentConfiguration =
                   [[TableViewCellContentConfiguration alloc] init];
               contentConfiguration.title = item.text;
               contentConfiguration.subtitle = item.detailText;

               if (item.loading) {
                 ActivityIndicatorContentConfiguration*
                     activityIndicatorConfiguration =
                         [[ActivityIndicatorContentConfiguration alloc] init];
                 activityIndicatorConfiguration.animating = YES;
                 contentConfiguration.leadingConfiguration =
                     activityIndicatorConfiguration;
               } else {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
                 NSString* symbolName = kGeminiBrandedLogoSymbol;
#else
            NSString* symbolName = kGeminiNonBrandedLogoSymbol;
#endif
                 contentConfiguration.leadingConfiguration =
                     autofill::AtMemoryCellIconConfiguration(symbolName);
               }

               cell.contentConfiguration = contentConfiguration;
               return cell;
             }];
  }

  NSDiffableDataSourceSnapshot<NSNumber*, AtMemorySearchItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierSearch),
  ]];

  AtMemorySearchItem* item = [[AtMemorySearchItem alloc] init];
  if (self.loading) {
    item.text = self.query;
    item.detailText = @"Reviewing info from Connected Apps";
    item.loading = YES;
  } else {
    item.text = self.query;
    item.detailText = @"Find and fill this with Gemini";
    item.loading = NO;
  }

  [snapshot appendItemsWithIdentifiers:@[ item ]
             intoSectionWithIdentifier:@(SectionIdentifierSearch)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  if (!self.loading) {
    [self.delegate searchViewControllerDidTapSearch:self];
  }
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.delegate searchViewController:self didTapLinkURL:URL];
}

@end
