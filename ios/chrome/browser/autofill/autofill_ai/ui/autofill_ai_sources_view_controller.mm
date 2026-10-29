// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_view_controller.h"

#import <cmath>

#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_section_header_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Width and height of the leading source icon in each row.
constexpr CGFloat kSourceAppIconSize = 24.0;

// Spacing between the title and subtitle in the navigation bar title view.
constexpr CGFloat kTitleStackSpacing = 2.0;

// Estimated height of a section header view before layout.
constexpr CGFloat kEstimatedSectionHeaderHeight = 28.0;

// Extra breathing room below the last grouped section in the sheet.
constexpr CGFloat kSheetBottomPadding = 16.0;

// Minimum table content height delta required to invalidate sheet detents.
constexpr CGFloat kContentHeightChangeEpsilon = 0.5;

// Identifier for the self-sizing custom sheet detent.
NSString* const kAutofillAiSourcesCustomDetentIdentifier =
    @"AutofillAiSourcesCustomDetentIdentifier";

// Format combining a row's accessible label with its new-tab context.
NSString* const kRowAccessibilityLabelFormat = @"%@, %@";

}  // namespace

@interface AutofillAiSourcesViewController () <UITableViewDataSource,
                                               UITableViewDelegate>
@end

@implementation AutofillAiSourcesViewController {
  NSString* _subtitle;
  NSArray<AutofillAiSourceGroup*>* _groups;
  UIImage* _externalLinkImage;
  CGFloat _lastTableViewContentHeight;
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

- (CGFloat)preferredHeightForContent {
  [self loadViewIfNeeded];
  [_tableView layoutIfNeeded];

  UINavigationBar* navigationBar = self.navigationController.navigationBar;
  CGFloat navigationBarHeight = navigationBar.frame.size.height;
  if (navigationBarHeight <= 0) {
    navigationBarHeight =
        [navigationBar sizeThatFits:self.view.bounds.size].height;
  }
  return navigationBarHeight + _tableView.contentSize.height +
         kSheetBottomPadding;
}

- (UISheetPresentationControllerDetent*)preferredHeightDetent {
  __weak __typeof(self) weakSelf = self;
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kAutofillAiSourcesCustomDetentIdentifier
                        resolver:^CGFloat(
                            id<UISheetPresentationControllerDetentResolutionContext>
                                context) {
                          AutofillAiSourcesViewController* strongSelf =
                              weakSelf;
                          // Fall back to automatic dimension rather than 0.0 if
                          // the view controller has already been deallocated
                          // during dismissal.
                          if (!strongSelf) {
                            return UISheetPresentationControllerAutomaticDimension;
                          }
                          return
                              [strongSelf resolveDetentValueForContext:context];
                        }];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  _externalLinkImage =
      DefaultAccessorySymbolConfigurationWithRegularWeight(SymbolExternalLink);

  [self setUpNavigationBar];
  [self setUpTableView];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  CGFloat contentHeight = _tableView.contentSize.height;
  if (std::abs(contentHeight - _lastTableViewContentHeight) <
      kContentHeightChangeEpsilon) {
    return;
  }
  _lastTableViewContentHeight = contentHeight;

  UISheetPresentationController* sheetPresentationController =
      self.navigationController.sheetPresentationController;
  // `sheetPresentationController` is nil when laid out outside a sheet
  // presentation (e.g., standalone in unit tests).
  if (!sheetPresentationController) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [sheetPresentationController animateChanges:^{
    [weakSelf.navigationController
            .sheetPresentationController invalidateDetents];
  }];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return _groups.count;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _groups[section].items.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView
                                                 forIndexPath:indexPath];

  AutofillAiSourceItem* item = _groups[indexPath.section].items[indexPath.row];

  ImageContentConfiguration* iconConfiguration =
      [[ImageContentConfiguration alloc] init];
  iconConfiguration.image = item.icon;
  iconConfiguration.imageSize =
      CGSizeMake(kSourceAppIconSize, kSourceAppIconSize);
  iconConfiguration.imageTintColor = [UIColor colorNamed:kTextSecondaryColor];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = item.title;
  if (item.subtitle.length > 0) {
    configuration.subtitle = item.subtitle;
  }
  configuration.leadingConfiguration = iconConfiguration;
  configuration.customAccessibilityLabel = [NSString
      stringWithFormat:kRowAccessibilityLabelFormat,
                       configuration.accessibilityLabel,
                       l10n_util::GetNSString(IDS_IOS_OPENS_IN_NEW_TAB)];

  UIImageView* accessoryImageView =
      [[UIImageView alloc] initWithImage:_externalLinkImage];
  accessoryImageView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  cell.accessoryView = accessoryImageView;
  cell.contentConfiguration = configuration;
  cell.accessibilityTraits |= UIAccessibilityTraitLink;

  return cell;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  AutofillAiSourcesSectionHeaderView* headerView =
      DequeueTableViewHeaderFooter<AutofillAiSourcesSectionHeaderView>(
          tableView);
  headerView.firstSection = (section == 0);
  [headerView configureWithTitle:_groups[section].title];
  return headerView;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return CGFLOAT_MIN;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return nil;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  AutofillAiSourceItem* item = _groups[indexPath.section].items[indexPath.row];
  [self.delegate sourcesViewController:self didSelectSourceItem:item];
}

#pragma mark - Private

// Resolves the custom sheet detent value capped at
// `context.maximumDetentValue`.
- (CGFloat)resolveDetentValueForContext:
    (id<UISheetPresentationControllerDetentResolutionContext>)context {
  return MIN([self preferredHeightForContent], context.maximumDetentValue);
}

// Configures the navigation bar title stack and close button.
- (void)setUpNavigationBar {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_TITLE);
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

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

// Creates and constrains the inset-grouped table view.
- (void)setUpTableView {
  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStyleInsetGrouped];
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.alwaysBounceVertical = NO;
  _tableView.accessibilityIdentifier = kAutofillAISourcesTableViewId;
  _tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  _tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  _tableView.sectionHeaderTopPadding = 0;
  _tableView.sectionHeaderHeight = UITableViewAutomaticDimension;
  _tableView.estimatedSectionHeaderHeight = kEstimatedSectionHeaderHeight;
  _tableView.sectionFooterHeight = 0;
  [TableViewCellContentConfiguration registerCellForTableView:_tableView];
  RegisterTableViewHeaderFooter<AutofillAiSourcesSectionHeaderView>(_tableView);
  [self.view addSubview:_tableView];

  PinToSafeArea(_tableView, self.view);
}

// Notifies the delegate that the close button was tapped.
- (void)handleCloseButton {
  [self.delegate sourcesViewControllerDidDismiss:self];
}

@end
