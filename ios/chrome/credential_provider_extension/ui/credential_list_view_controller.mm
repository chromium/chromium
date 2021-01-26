// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* kHeaderIdentifier = @"clvcHeader";
NSString* kCellIdentifier = @"clvcCell";

const CGFloat kHeaderHeight = 70;
}

// This cell just adds a simple hover pointer interaction to the TableViewCell.
@interface CredentialListCell : UITableViewCell
@end

@implementation CredentialListCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    if (@available(iOS 13.4, *)) {
      [self addInteraction:[[ViewPointerInteraction alloc] init]];
    }
  }
  return self;
}

@end

@interface CredentialListViewController () <UITableViewDataSource,
                                            UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

// Current list of suggested passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* suggestedPasswords;

// Current list of all passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* allPasswords;

@end

@implementation CredentialListViewController

@synthesize delegate;

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CREDENTIAL_LIST_TITLE",
                        @"AutoFill Chrome Password");
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.navigationItem.rightBarButtonItem = [self navigationCancelButton];

  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.searchResultsUpdater = self;
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.searchBar.barTintColor =
      [UIColor colorNamed:kBackgroundColor];
  // Add en empty space at the bottom of the list, the size of the search bar,
  // to allow scrolling up enough to see last result, otherwise it remains
  // hidden under the accessories.
  self.tableView.tableFooterView =
      [[UIView alloc] initWithFrame:self.searchController.searchBar.frame];
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  self.navigationController.navigationBar.barTintColor =
      [UIColor colorNamed:kBackgroundColor];
  self.navigationController.navigationBar.tintColor =
      [UIColor colorNamed:kBlueColor];
  self.navigationController.navigationBar.shadowImage = [[UIImage alloc] init];

  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this class the presentation context so that the search
  // controller does not present on top of the navigation controller.
  self.definesPresentationContext = YES;
  [self.tableView registerClass:[UITableViewHeaderFooterView class]
      forHeaderFooterViewReuseIdentifier:kHeaderIdentifier];
}

#pragma mark - CredentialListConsumer

- (void)presentSuggestedPasswords:(NSArray<id<Credential>>*)suggested
                     allPasswords:(NSArray<id<Credential>>*)all {
  self.suggestedPasswords = suggested;
  self.allPasswords = all;
  [self.tableView reloadData];
  [self.tableView layoutIfNeeded];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [self numberOfSections];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if ([self isEmptyTable]) {
    return 0;
  } else if ([self isSuggestedPasswordSection:section]) {
    return self.suggestedPasswords.count;
  } else {
    return self.allPasswords.count;
  }
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  if ([self isEmptyTable]) {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NO_SEARCH_RESULTS",
                             @"No search results found");
  } else if ([self isSuggestedPasswordSection:section]) {
    if (self.suggestedPasswords.count > 1) {
      return NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_SUGGESTED_PASSWORDS",
          @"Suggested Passwords");
    } else {
      return NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_SUGGESTED_PASSWORD",
          @"Suggested Password");
    }
  } else {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_ALL_PASSWORDS",
                             @"All Passwords");
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kCellIdentifier];
  if (!cell) {
    cell =
        [[CredentialListCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:kCellIdentifier];
    cell.accessoryView = [self infoIconButton];
  }

  id<Credential> credential = [self credentialForIndexPath:indexPath];
  cell.textLabel.text = credential.serviceName;
  cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  cell.detailTextLabel.text = credential.user;
  cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  cell.selectionStyle = UITableViewCellSelectionStyleDefault;
  cell.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  return cell;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UITableViewHeaderFooterView* view = [self.tableView
      dequeueReusableHeaderFooterViewWithIdentifier:kHeaderIdentifier];
  view.textLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  view.contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  return view;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return kHeaderHeight;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  UpdateUMACountForKey(app_group::kCredentialExtensionPasswordUseCount);
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  [self.delegate userSelectedCredential:credential];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  if (searchController.searchBar.text.length) {
    UpdateUMACountForKey(app_group::kCredentialExtensionSearchCount);
  }
  [self.delegate updateResultsWithFilter:searchController.searchBar.text];
}

#pragma mark - Private

// Creates a cancel button for the navigation item.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self.delegate
                           action:@selector(navigationCancelButtonWasPressed:)];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  return cancelButton;
}

// Creates a button to be displayed as accessory of the password row item.
- (UIView*)infoIconButton {
  UIImage* image = [UIImage imageNamed:@"info_icon"];
  image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  HighlightButton* button = [HighlightButton buttonWithType:UIButtonTypeCustom];
  button.frame = CGRectMake(0.0, 0.0, image.size.width, image.size.height);
  [button setBackgroundImage:image forState:UIControlStateNormal];
  [button setTintColor:[UIColor colorNamed:kBlueColor]];
  [button addTarget:self
                action:@selector(infoIconButtonTapped:event:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityLabel = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_SHOW_DETAILS_ACCESSIBILITY_LABEL",
      @"Show Details.");

  if (@available(iOS 13.4, *)) {
    button.pointerInteractionEnabled = YES;
    button.pointerStyleProvider = ^UIPointerStyle*(
        UIButton* button, __unused UIPointerEffect* proposedEffect,
        __unused UIPointerShape* proposedShape) {
      UITargetedPreview* preview =
          [[UITargetedPreview alloc] initWithView:button];
      UIPointerHighlightEffect* effect =
          [UIPointerHighlightEffect effectWithPreview:preview];
      UIPointerShape* shape =
          [UIPointerShape shapeWithRoundedRect:button.frame
                                  cornerRadius:button.frame.size.width / 2];
      return [UIPointerStyle styleWithEffect:effect shape:shape];
    };
  }

  return button;
}

// Called when info icon is tapped.
- (void)infoIconButtonTapped:(id)sender event:(id)event {
  CGPoint hitPoint =
      [base::mac::ObjCCastStrict<UIButton>(sender) convertPoint:CGPointZero
                                                         toView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:hitPoint];
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  [self.delegate showDetailsForCredential:credential];
}

// Returns number of sections to display based on |suggestedPasswords| and
// |allPasswords|. If no sections with data, returns 1 for the 'no data' banner.
- (int)numberOfSections {
  if ([self.suggestedPasswords count] == 0 || [self.allPasswords count] == 0) {
    return 1;
  }
  return 2;
}

// Returns YES if there is no data to display.
- (BOOL)isEmptyTable {
  return [self.suggestedPasswords count] == 0 && [self.allPasswords count] == 0;
}

// Returns YES if given section is for suggested passwords.
- (BOOL)isSuggestedPasswordSection:(int)section {
  int sections = [self numberOfSections];
  if ((sections == 2 && section == 0) ||
      (sections == 1 && self.suggestedPasswords.count)) {
    return YES;
  } else {
    return NO;
  }
}

- (id<Credential>)credentialForIndexPath:(NSIndexPath*)indexPath {
  if ([self isSuggestedPasswordSection:indexPath.section]) {
    return self.suggestedPasswords[indexPath.row];
  } else {
    return self.allPasswords[indexPath.row];
  }
}

@end
