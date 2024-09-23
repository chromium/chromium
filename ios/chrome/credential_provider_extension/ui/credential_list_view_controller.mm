// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/favicon_table_view_cell.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_global_header_view.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_header_view.h"

namespace {

// Reuse Identifiers for table views.
NSString* kHeaderIdentifier = @"clvcHeader";
NSString* kCredentialCellIdentifier = @"clvcCredentialCell";
NSString* kNewPasswordCellIdentifier = @"clvcNewPasswordCell";

const CGFloat kNewCredentialHeaderHeight = 35;
// Add extra space to offset the top of the table view from the search bar.
const CGFloat kTableViewTopSpace = 8;

UIColor* BackgroundColor() {
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}
}

// This cell just adds a simple hover pointer interaction to the TableViewCell.
@interface CredentialListCell : FaviconTableViewCell
@end

@implementation CredentialListCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    [self addInteraction:[[ViewPointerInteraction alloc] init]];
  }
  return self;
}

@end

@interface CredentialListViewController () <UITableViewDataSource,
                                            UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

// Current list of suggested credentials.
@property(nonatomic, copy) NSArray<id<Credential>>* suggestedCredentials;

// Current list of all credentials.
@property(nonatomic, copy) NSArray<id<Credential>>* allCredentials;

// Indicates if the option to create a new password should be presented.
@property(nonatomic, assign) BOOL showNewPasswordOption;

// FaviconAttributes object with the default world icon as fallback.
@property(nonatomic, strong) FaviconAttributes* defaultWorldIconAttributes;

@end

@implementation CredentialListViewController

@synthesize delegate;

- (instancetype)init {
  UITableViewStyle style = UITableViewStyleInsetGrouped;
  self = [super initWithStyle:style];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_CREDENTIAL_LIST_BRANDED_TITLE",
      @"Google Password Manager");

  self.view.backgroundColor = BackgroundColor();
  self.navigationItem.leftBarButtonItem = [self navigationCancelButton];

  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.searchResultsUpdater = self;
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.searchBar.barTintColor = BackgroundColor();
  // Add en empty space at the bottom of the list, the size of the search bar,
  // to allow scrolling up enough to see last result, otherwise it remains
  // hidden under the accessories.
  self.tableView.tableFooterView =
      [[UIView alloc] initWithFrame:self.searchController.searchBar.frame];
  self.tableView.contentInset = UIEdgeInsetsMake(kTableViewTopSpace, 0, 0, 0);
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  UINavigationBarAppearance* appearance =
      [[UINavigationBarAppearance alloc] init];
  [appearance configureWithDefaultBackground];
  appearance.backgroundColor = BackgroundColor();
  self.navigationItem.scrollEdgeAppearance = appearance;
  self.navigationController.navigationBar.tintColor =
      [UIColor colorNamed:kBlueColor];

  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this class the presentation context so that the search
  // controller does not present on top of the navigation controller.
  self.definesPresentationContext = YES;
  [self.tableView registerClass:[UITableViewHeaderFooterView class]
      forHeaderFooterViewReuseIdentifier:kHeaderIdentifier];
  [self.tableView registerClass:[CredentialListHeaderView class]
      forHeaderFooterViewReuseIdentifier:CredentialListHeaderView.reuseID];
  [self.tableView registerClass:[CredentialListGlobalHeaderView class]
      forHeaderFooterViewReuseIdentifier:CredentialListGlobalHeaderView
                                             .reuseID];
}

#pragma mark - CredentialListConsumer

- (void)presentSuggestedCredentials:(NSArray<id<Credential>>*)suggested
                     allCredentials:(NSArray<id<Credential>>*)all
                      showSearchBar:(BOOL)showSearchBar
              showNewPasswordOption:(BOOL)showNewPasswordOption {
  self.suggestedCredentials = suggested;
  self.allCredentials = all;
  self.showNewPasswordOption = showNewPasswordOption;
  [self.tableView reloadData];
  [self.tableView layoutIfNeeded];

  // Remove or add the search controller depending on whether there are
  // passwords to search.
  if (showSearchBar) {
    self.navigationItem.searchController = self.searchController;
  } else {
    if (self.navigationItem.searchController.isActive) {
      self.navigationItem.searchController.active = NO;
    }
    self.navigationItem.searchController = nil;
  }
}

- (void)setTopPrompt:(NSString*)prompt {
  self.navigationController.navigationBar.topItem.prompt = prompt;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [self numberOfSections];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if ([self isEmptyTable]) {
    return 0;
  } else if ([self isSuggestedCredentialSection:section]) {
    return [self numberOfRowsInSuggestedCredentialSection];
  } else {
    return self.allCredentials.count;
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathNewPasswordRow:indexPath]) {
    UITableViewCell* cell = [tableView
        dequeueReusableCellWithIdentifier:kNewPasswordCellIdentifier];
    if (!cell) {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                    reuseIdentifier:kNewPasswordCellIdentifier];
    }
    cell.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    cell.textLabel.text =
        NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CREATE_PASSWORD_ROW",
                          @"Add New Password");
    cell.textLabel.textColor = [UIColor colorNamed:kBlueColor];
    return cell;
  }

  id<Credential> credential = [self credentialForIndexPath:indexPath];

  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kCredentialCellIdentifier];

    if (!cell) {
      cell =
          [[CredentialListCell alloc] initWithStyle:UITableViewCellStyleDefault
                                    reuseIdentifier:kCredentialCellIdentifier];
      cell.accessoryView = [self infoIconButton];
    }

    CredentialListCell* credentialCell =
        base::apple::ObjCCastStrict<CredentialListCell>(cell);

    credentialCell.textLabel.text = credential.serviceName;
    credentialCell.detailTextLabel.text = credential.username;
    credentialCell.uniqueIdentifier = credential.serviceIdentifier;
    credentialCell.selectionStyle = UITableViewCellSelectionStyleDefault;
    credentialCell.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    credentialCell.accessibilityTraits |= UIAccessibilityTraitButton;

    // Load favicon.
    if (credential.favicon) {
      // Load the favicon from disk.
      [self loadFaviconAtIndexPath:indexPath forCell:cell];
    }

    // Use the default world icon as fallback.
    if (!self.defaultWorldIconAttributes) {
      self.defaultWorldIconAttributes = [FaviconAttributes
          attributesWithImage:
              [[UIImage imageNamed:@"default_world_favicon"]
                  imageWithTintColor:[UIColor colorNamed:kTextQuaternaryColor]
                       renderingMode:UIImageRenderingModeAlwaysOriginal]];
    }
    [credentialCell.faviconView
        configureWithAttributes:self.defaultWorldIconAttributes];
    return credentialCell;
}

// Asynchronously loads favicon for given index path. The loads are cancelled
// upon cell reuse automatically.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
                       forCell:(UITableViewCell*)cell {
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  DCHECK(credential);
  DCHECK(cell);
  CredentialListCell* credentialCell =
      base::apple::ObjCCastStrict<CredentialListCell>(cell);
  NSString* serviceIdentifier = credential.serviceIdentifier;

  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
    NSURL* filePath = [app_group::SharedFaviconAttributesFolder()
        URLByAppendingPathComponent:credential.favicon
                        isDirectory:NO];
    NSError* error = nil;
    NSData* data = [NSData dataWithContentsOfURL:filePath
                                         options:0
                                           error:&error];
    if (data && !error) {
      NSKeyedUnarchiver* unarchiver =
          [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
      unarchiver.requiresSecureCoding = NO;
      FaviconAttributes* attributes =
          [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
      // Only set favicon if the cell hasn't been reused.
      if ([credentialCell.uniqueIdentifier isEqualToString:serviceIdentifier]) {
        // Update the UI on the main thread.
        dispatch_async(dispatch_get_main_queue(), ^{
          if (attributes) {
            [credentialCell.faviconView configureWithAttributes:attributes];
          }
        });
      }
    }
  });
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  if ([self isGlobalHeaderSection:section]) {
    return [self.tableView dequeueReusableHeaderFooterViewWithIdentifier:
                               CredentialListGlobalHeaderView.reuseID];
  }
  CredentialListHeaderView* view = [self.tableView
      dequeueReusableHeaderFooterViewWithIdentifier:CredentialListHeaderView
                                                        .reuseID];
  view.headerTextLabel.text = [self titleForHeaderInSection:section];
  view.contentView.backgroundColor = BackgroundColor();
  return view;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self isGlobalHeaderSection:section]) {
    return UITableViewAutomaticDimension;
  }
  if ([self isSuggestedCredentialSection:section]) {
    return 0;
  }
  return kNewCredentialHeaderHeight;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathNewPasswordRow:indexPath]) {
    [self.delegate newPasswordWasSelected];
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  if (!credential) {
    return;
  }
  UpdateUMACountForKey(credential.isPasskey
                           ? app_group::kCredentialExtensionPasskeyUseCount
                           : app_group::kCredentialExtensionPasswordUseCount);
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

// Creates a button to be displayed as accessory of the credential row item.
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

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = ^UIPointerStyle*(
      UIButton* theButton, __unused UIPointerEffect* proposedEffect,
      __unused UIPointerShape* proposedShape) {
    UITargetedPreview* preview =
        [[UITargetedPreview alloc] initWithView:theButton];
    UIPointerHighlightEffect* effect =
        [UIPointerHighlightEffect effectWithPreview:preview];
    UIPointerShape* shape =
        [UIPointerShape shapeWithRoundedRect:theButton.frame
                                cornerRadius:theButton.frame.size.width / 2];
    return [UIPointerStyle styleWithEffect:effect shape:shape];
  };

  return button;
}

// Called when info icon is tapped.
- (void)infoIconButtonTapped:(id)sender event:(id)event {
  CGPoint hitPoint = [base::apple::ObjCCastStrict<UIButton>(sender)
      convertPoint:CGPointZero
            toView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:hitPoint];
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  if (!credential) {
    return;
  }
  [self.delegate showDetailsForCredential:credential];
}

// Returns number of sections to display based on `suggestedCredentials` and
// `allCredentials`. If no sections with data, returns 1 for the 'no data'
// banner.
- (int)numberOfSections {
  if ([self numberOfRowsInSuggestedCredentialSection] == 0 ||
      [self.allCredentials count] == 0) {
    return 1;
  }
  return 2;
}

// Returns YES if there is no data to display.
- (BOOL)isEmptyTable {
  return [self numberOfRowsInSuggestedCredentialSection] == 0 &&
         [self.allCredentials count] == 0;
}

// Returns YES if given section is for suggested credentials.
- (BOOL)isSuggestedCredentialSection:(int)section {
  int sections = [self numberOfSections];
  if ((sections == 2 && section == 0) ||
      (sections == 1 && [self numberOfRowsInSuggestedCredentialSection])) {
    return YES;
  } else {
    return NO;
  }
}

// Returns YES if given section is for global header.
- (BOOL)isGlobalHeaderSection:(int)section {
  return section == 0 && ![self isEmptyTable];
}

// Returns the credential at the passed index.
- (id<Credential>)credentialForIndexPath:(NSIndexPath*)indexPath {
  if ([self isSuggestedCredentialSection:indexPath.section]) {
    if (indexPath.row >=
        base::checked_cast<NSInteger>(self.suggestedCredentials.count)) {
      return nil;
    }
    return self.suggestedCredentials[indexPath.row];
  } else {
    if (indexPath.row >=
        base::checked_cast<NSInteger>(self.allCredentials.count)) {
      return nil;
    }
    return self.allCredentials[indexPath.row];
  }
}

// Returns true if the passed index corresponds to the Create New Password Cell.
- (BOOL)isIndexPathNewPasswordRow:(NSIndexPath*)indexPath {
  if ([self isSuggestedCredentialSection:indexPath.section]) {
    return indexPath.row == NSInteger(self.suggestedCredentials.count);
  }
  return NO;
}

// Returns the number of rows in suggested credentials section.
- (NSUInteger)numberOfRowsInSuggestedCredentialSection {
  return
      [self.suggestedCredentials count] + (self.showNewPasswordOption ? 1 : 0);
}

// Returns the title of the given section
- (NSString*)titleForHeaderInSection:(NSInteger)section {
  if ([self isEmptyTable]) {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NO_SEARCH_RESULTS",
                             @"No search results found");
  } else if ([self isSuggestedCredentialSection:section]) {
    return nil;
  } else if ([self.allCredentials count] > 0 &&
             self.allCredentials[0].isPasskey) {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_ALL_PASSKEYS",
                             @"All Passkeys");
  } else {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_ALL_PASSWORDS",
                             @"All Passwords");
  }
}

@end
