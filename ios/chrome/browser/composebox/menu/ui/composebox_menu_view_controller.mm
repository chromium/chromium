// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_section.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The height of the attachments stack view.
const CGFloat kAttachmentStackViewHeight = 80.0f;

// The spacing of the attachments stack view.
const CGFloat kAttachmentStackViewSpacing = 6.0f;

// Insets for the safe area (top, left, bottom, right).
const UIEdgeInsets kSafeAreaInsets = {20.0, 15.0, 0.0, 15.0};

// The reuse identifier for the composebox menu table view cell.
NSString* const kComposeboxMenuCellIdentifier = @"ComposeboxMenuCell";

}  // namespace

@interface ComposeboxMenuViewController () <UITableViewDataSource,
                                            UITableViewDelegate>
@end

@implementation ComposeboxMenuViewController {
  // The table view displaying the composebox menu tools and actions.
  UITableView* _tableView;
  // The stack view containing the attachments
  UIStackView* _attachmentStackView;
  // The sections to display in the table view.
  NSArray<ComposeboxMenuSection*>* _sections;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setAdditionalSafeAreaInsets:kSafeAreaInsets];
  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];

  [self setUpSections];
  [self setUpTableView];

  UIView* headerView =
      [self headerViewWithItems:[self availableAttachmentViews]];
  _tableView.tableHeaderView = headerView;
}

- (void)setUpSections {
  ComposeboxMenuItem* aimItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION)
              image:CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                              kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAIM];
  ComposeboxMenuItem* createImageItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION)
              image:GetBananaIcon(kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kCreateImage];
  ComposeboxMenuItem* deepSearchItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION)
              image:CustomSymbolWithPointSize(kDeepSearchSymbol,
                                              kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kDeepSearch];
  ComposeboxMenuItem* canvasItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION)
              image:CustomSymbolWithPointSize(kDocumentBadgeSpark,
                                              kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kCanvas];

  ComposeboxMenuItem* autoItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO)
              image:DefaultSymbolWithPointSize(
                        kClockArrowTriangleheadCounterclockwiseRotate90Symbol,
                        kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kModelAuto];
  ComposeboxMenuItem* thinkingItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_THINKING)
              image:DefaultSymbolWithPointSize(kClockSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kModelThinking];

  ComposeboxMenuSection* toolsSection = [[ComposeboxMenuSection alloc]
      initWithTitle:@"Tools"  // TODO (crbug.com/504976247): Add the translated
                              // string.
              items:@[ aimItem, createImageItem, deepSearchItem, canvasItem ]];

  ComposeboxMenuSection* modelsSection = [[ComposeboxMenuSection alloc]
      initWithTitle:l10n_util::GetNSStringF(
                        IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_TITLE, u"3")
              items:@[ autoItem, thinkingItem ]];

  _sections = @[ toolsSection, modelsSection ];
}

- (void)setUpTableView {
  _tableView = [[UITableView alloc] initWithFrame:self.view.bounds
                                            style:UITableViewStyleInsetGrouped];
  [_tableView registerClass:[UITableViewCell class]
      forCellReuseIdentifier:kComposeboxMenuCellIdentifier];
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.dataSource = self;
  _tableView.delegate = self;
  _tableView.backgroundColor = [UIColor colorNamed:kGrey100Color];

  [self.view addSubview:_tableView];

  AddSameConstraints(_tableView, self.view);

  /// Remove extra space from UITableViewWrapperView.
  _tableView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
  _tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  _tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
}

#pragma mark - Private

// Returns the available attachment views.
- (NSArray<UIView*>*)availableAttachmentViews {
  UIView* tabsAttachment =
      [self attachmentViewWithTitle:l10n_util::GetNSString(
                                        IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
                             symbol:DefaultSymbolWithPointSize(
                                        kNewTabGroupActionSymbol,
                                        kSymbolActionPointSize)];
  UIView* cameraAttachment = [self
      attachmentViewWithTitle:l10n_util::GetNSString(
                                  IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
                       symbol:DefaultSymbolWithPointSize(
                                  kSystemCameraSymbol, kSymbolActionPointSize)];
  UIView* galleryAttachment =
      [self attachmentViewWithTitle:l10n_util::GetNSString(
                                        IDS_IOS_COMPOSEBOX_GALLERY_ACTION)
                             symbol:DefaultSymbolWithPointSize(
                                        kPhotoOnRectangleAngled,
                                        kSymbolActionPointSize)];
  UIView* filesAttachment =
      [self attachmentViewWithTitle:l10n_util::GetNSString(
                                        IDS_IOS_COMPOSEBOX_FILES_ACTION)
                             symbol:DefaultSymbolWithPointSize(
                                        kFolderSymbol, kSymbolActionPointSize)];

  return
      @[ tabsAttachment, cameraAttachment, galleryAttachment, filesAttachment ];
}

// Sets up the container for the attachment options.
- (UIView*)headerViewWithItems:(NSArray<UIView*>*)items {
  _attachmentStackView = [[UIStackView alloc] initWithArrangedSubviews:items];
  _attachmentStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _attachmentStackView.distribution = UIStackViewDistributionFillEqually;
  _attachmentStackView.alignment = UIStackViewAlignmentFill;
  _attachmentStackView.axis = UILayoutConstraintAxisHorizontal;
  _attachmentStackView.spacing = kAttachmentStackViewSpacing;

  CGFloat height =
      kAttachmentStackViewHeight + kSafeAreaInsets.top + kSafeAreaInsets.bottom;
  UIView* headerView = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, self.view.bounds.size.width, height)];
  headerView.backgroundColor = [UIColor clearColor];
  [headerView addSubview:_attachmentStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_attachmentStackView.heightAnchor
        constraintEqualToConstant:kAttachmentStackViewHeight],
    [_attachmentStackView.leadingAnchor
        constraintEqualToAnchor:headerView.leadingAnchor
                       constant:kSafeAreaInsets.left],
    [_attachmentStackView.trailingAnchor
        constraintEqualToAnchor:headerView.trailingAnchor
                       constant:-kSafeAreaInsets.right],
    [_attachmentStackView.topAnchor
        constraintEqualToAnchor:headerView.topAnchor
                       constant:kSafeAreaInsets.top],
  ]];

  return headerView;
}

// Create an attachment view with the given title and symbol.
- (ComposeboxMenuAttachmentView*)attachmentViewWithTitle:(NSString*)title
                                                  symbol:(UIImage*)symbol {
  ComposeboxMenuAttachmentView* attachmentView =
      [[ComposeboxMenuAttachmentView alloc] init];
  attachmentView.translatesAutoresizingMaskIntoConstraints = NO;
  attachmentView.title = title;
  attachmentView.image =
      SymbolWithPalette(symbol, @[ [UIColor colorNamed:kTextPrimaryColor] ]);
  return attachmentView;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return _sections.count;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _sections[section].items.count;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  return _sections[section].title;
}

- (void)tableView:(UITableView*)tableView
    willDisplayHeaderView:(UIView*)view
               forSection:(NSInteger)section {
  if ([view isKindOfClass:[UITableViewHeaderFooterView class]]) {
    UITableViewHeaderFooterView* header = (UITableViewHeaderFooterView*)view;
    header.textLabel.font = [UIFont systemFontOfSize:16.0
                                              weight:UIFontWeightBold];
    header.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView
      dequeueReusableCellWithIdentifier:kComposeboxMenuCellIdentifier];

  ComposeboxMenuItem* item = _sections[indexPath.section].items[indexPath.row];

  UIListContentConfiguration* configuration =
      [cell defaultContentConfiguration];
  configuration.text = item.title;
  configuration.image = item.image;
  configuration.imageProperties.tintColor =
      [UIColor colorNamed:kTextPrimaryColor];
  cell.contentConfiguration = configuration;

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  ComposeboxMenuItem* item = _sections[indexPath.section].items[indexPath.row];

  // TODO (crbug.com/505269628): Implement menu items selection.
  switch (item.type) {
    case ComposeboxMenuItemType::kAIM:
      break;
    case ComposeboxMenuItemType::kCreateImage:
      break;
    case ComposeboxMenuItemType::kDeepSearch:
      break;
    case ComposeboxMenuItemType::kCanvas:
      break;
    case ComposeboxMenuItemType::kModelAuto:
      break;
    case ComposeboxMenuItemType::kModelThinking:
      break;
    case ComposeboxMenuItemType::kUnknown:
      break;
  }
}

@end
