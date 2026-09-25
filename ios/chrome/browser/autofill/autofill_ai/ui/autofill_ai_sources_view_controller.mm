// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_view_controller.h"

#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Size of the source app icon displayed in each row.
constexpr CGFloat kSourceAppIconSize = 24.0;

// Corner radius of the source app icon.
constexpr CGFloat kSourceAppIconCornerRadius = 4.0;

// Point size for the external link trailing symbol.
constexpr CGFloat kExternalLinkSymbolSize = 16.0;

// Spacing between the title and subtitle in the navigation bar title view.
constexpr CGFloat kTitleStackSpacing = 2.0;

// Reuse identifier for the source table view cell.
NSString* const kSourceCellReuseIdentifier = @"AutofillAiSourceCell";

}  // namespace

@interface AutofillAiSourcesViewController () <UITableViewDataSource,
                                               UITableViewDelegate>
@end

@implementation AutofillAiSourcesViewController {
  NSString* _subtitle;
  NSArray<AutofillAiSourceGroup*>* _groups;
  UIImage* _externalLinkImage;
}

#pragma mark - Public

- (instancetype)initWithSubtitle:(NSString*)subtitle
                          groups:(NSArray<AutofillAiSourceGroup*>*)groups {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _subtitle = [subtitle copy];
    _groups = [groups copy];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  _externalLinkImage =
      SymbolWithPointSize(SymbolExternalLink, kExternalLinkSymbolSize);

  [self setUpNavigationBar];
  [self setUpTableView];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return _groups.count;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _groups[section].items.count;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  return _groups[section].title;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kSourceCellReuseIdentifier
                                      forIndexPath:indexPath];

  AutofillAiSourceItem* item = _groups[indexPath.section].items[indexPath.row];

  UIListContentConfiguration* config = [cell defaultContentConfiguration];
  config.text = item.title;
  config.textProperties.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  config.textProperties.color = [UIColor colorNamed:kTextPrimaryColor];
  if (item.subtitle.length > 0) {
    config.secondaryText = item.subtitle;
    config.secondaryTextProperties.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    config.secondaryTextProperties.color =
        [UIColor colorNamed:kTextSecondaryColor];
  }
  config.image = item.icon;
  config.imageProperties.maximumSize =
      CGSizeMake(kSourceAppIconSize, kSourceAppIconSize);
  config.imageProperties.cornerRadius = kSourceAppIconCornerRadius;
  cell.contentConfiguration = config;

  UIImageView* accessoryImageView =
      [[UIImageView alloc] initWithImage:_externalLinkImage];
  accessoryImageView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  cell.accessoryView = accessoryImageView;

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  AutofillAiSourceItem* item = _groups[indexPath.section].items[indexPath.row];
  [self.delegate sourcesViewController:self didSelectSourceItem:item];
}

#pragma mark - Private

- (void)setUpNavigationBar {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_TITLE);
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.adjustsFontForContentSizeCategory = YES;

  if (_subtitle.length > 0) {
    UILabel* subtitleLabel = [[UILabel alloc] init];
    subtitleLabel.text = _subtitle;
    subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    subtitleLabel.textAlignment = NSTextAlignmentCenter;
    subtitleLabel.adjustsFontForContentSizeCategory = YES;

    UIStackView* titleStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ titleLabel, subtitleLabel ]];
    titleStack.axis = UILayoutConstraintAxisVertical;
    titleStack.alignment = UIStackViewAlignmentCenter;
    titleStack.spacing = kTitleStackSpacing;
    self.navigationItem.titleView = titleStack;
  } else {
    self.navigationItem.titleView = titleLabel;
  }

  UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(handleCloseButton)];
  closeButton.accessibilityIdentifier = kAutofillAISourcesCancelButtonId;
  self.navigationItem.rightBarButtonItem = closeButton;
}

- (void)setUpTableView {
  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStyleInsetGrouped];
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.accessibilityIdentifier = kAutofillAISourcesTableViewId;
  _tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  _tableView.sectionHeaderTopPadding = 0;
  [_tableView registerClass:[UITableViewCell class]
      forCellReuseIdentifier:kSourceCellReuseIdentifier];
  [self.view addSubview:_tableView];

  PinToSafeArea(_tableView, self.view);
}

- (void)handleCloseButton {
  [self.delegate sourcesViewControllerDidDismiss:self];
}

@end
