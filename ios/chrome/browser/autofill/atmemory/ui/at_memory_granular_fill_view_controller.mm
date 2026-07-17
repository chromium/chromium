// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Section identifiers for the granular fill table view.
enum SectionIdentifier {
  kGranularFillFieldsSection = 0,
};

// Item types for the granular fill table view.
enum ItemType {
  kAttributeItemType = 0,
};

}  // namespace

@implementation AtMemoryGranularFillViewController {
  // The data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, TableViewItem*>* _dataSource;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsSelection = NO;
  [self setupLinkFooter];
  [self loadModel];
}

- (void)setupLinkFooter {
  UIView* footerView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 320, 100)];
  UIStackView* linkStack = [[UIStackView alloc] init];
  linkStack.translatesAutoresizingMaskIntoConstraints = NO;
  linkStack.axis = UILayoutConstraintAxisVertical;
  linkStack.spacing = 16.0;
  linkStack.alignment = UIStackViewAlignmentLeading;
  [footerView addSubview:linkStack];

  UIButton* suggestedLink =
      [self createLinkButtonWithTitle:@"Suggested by Gemini"];
  [linkStack addArrangedSubview:suggestedLink];

  UIButton* manageLink =
      [self createLinkButtonWithTitle:@"Manage enhanced autofill"];
  [linkStack addArrangedSubview:manageLink];

  [NSLayoutConstraint activateConstraints:@[
    [linkStack.topAnchor constraintEqualToAnchor:footerView.topAnchor
                                        constant:16.0],
    [linkStack.leadingAnchor constraintEqualToAnchor:footerView.leadingAnchor
                                            constant:16.0],
    [linkStack.trailingAnchor constraintEqualToAnchor:footerView.trailingAnchor
                                             constant:-16.0],
    [linkStack.bottomAnchor constraintEqualToAnchor:footerView.bottomAnchor
                                           constant:-16.0],
  ]];

  self.tableView.tableFooterView = footerView;
}

- (void)setItems:(NSArray<AtMemoryGranularFillItem*>*)items {
  _items = [items copy];
  if (self.isViewLoaded) {
    [self loadModel];
  }
}

- (void)loadModel {
  if (!_dataSource) {
    _dataSource = [[UITableViewDiffableDataSource alloc]
        initWithTableView:self.tableView
             cellProvider:^UITableViewCell*(UITableView* tableView,
                                            NSIndexPath* indexPath,
                                            TableViewItem* item) {
               LegacyTableViewCell* cell = [item cellForTableView:tableView];
               [item configureCell:cell];
               return cell;
             }];
  }

  NSDiffableDataSourceSnapshot<NSNumber*, TableViewItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(kGranularFillFieldsSection),
  ]];

  for (AtMemoryGranularFillItem* item in self.items) {
    item.target = self;
    item.action = @selector(didTapChip:);
  }

  [snapshot appendItemsWithIdentifiers:self.items ?: @[]
             intoSectionWithIdentifier:@(kGranularFillFieldsSection)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - Actions

- (void)didTapChip:(UIButton*)sender {
  [self.delegate granularFillViewController:self
                           didSelectContent:sender.configuration.title];
}

#pragma mark - Private

// Creates and returns a link-style button with the given title.
- (UIButton*)createLinkButtonWithTitle:(NSString*)title {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.title = title;
  config.baseForegroundColor = [UIColor colorNamed:kBlueColor];
  config.contentInsets = NSDirectionalEdgeInsetsZero;
  button.configuration = config;
  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;

  return button;
}

@end
