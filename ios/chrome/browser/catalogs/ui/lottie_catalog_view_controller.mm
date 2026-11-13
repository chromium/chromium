// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/lottie_catalog_view_controller.h"

#import "ios/chrome/browser/catalogs/ui/lottie_catalog_item.h"
#import "ios/chrome/browser/catalogs/ui/lottie_catalog_items.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_configuration.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_with_bricks_view_controller.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/title_brick_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Sections used in the Lottie Catalog page.
enum SectionIdentifier {
  kSectionIdentifierConfiguration = kSectionIdentifierEnumZero,
  kSectionIdentifierAction,
};

// Item types used in the Lottie Catalog page.
enum ItemType {
  kItemTypeAnimation = kItemTypeEnumZero,
  kItemTypeNavigationController,
  kItemTypePresent,
};

// Title of the catalog page.
NSString* const kCatalogTitle = @"Lottie Catalog";
// Title of the button dismissing a previewed animated promo.
NSString* const kCloseActionTitle = @"Close";
// Title of the switch wrapping the promo in a navigation controller.
NSString* const kNavigationControllerSwitchTitle = @"In navigation controller";
// Title of the row presenting the selected animation.
NSString* const kPresentTitle = @"Present";

}  // namespace

@interface LottieCatalogViewController () <ButtonStackActionDelegate>
@end

@implementation LottieCatalogViewController {
  // All the animations of the catalog, sorted by feature.
  NSArray<LottieCatalogItem*>* _items;
  // The animation presented when tapping the Present row.
  LottieCatalogItem* _selectedItem;
  // Whether the promo is wrapped in a navigation controller when presented.
  BOOL _presentsInNavigationController;
  // Row displaying the selected animation.
  TableViewDetailTextItem* _animationItem;
  // Row of the switch wrapping the promo in a navigation controller.
  TableViewSwitchItem* _navigationControllerItem;
  // Accessory button of the animation row, opening the animation menu.
  UIButton* _animationMenuButton;
}

- (instancetype)init {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];
  if (self) {
    _items = CreateLottieCatalogItems();
    _selectedItem = _items.firstObject;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = kCatalogTitle;
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  [model addSectionWithIdentifier:kSectionIdentifierConfiguration];
  _animationItem =
      [[TableViewDetailTextItem alloc] initWithType:kItemTypeAnimation];
  [self updateAnimationItem];
  [model addItem:_animationItem
      toSectionWithIdentifier:kSectionIdentifierConfiguration];
  _navigationControllerItem = [self navigationControllerItem];
  [model addItem:_navigationControllerItem
      toSectionWithIdentifier:kSectionIdentifierConfiguration];

  TableViewTextItem* presentItem =
      [[TableViewTextItem alloc] initWithType:kItemTypePresent];
  presentItem.text = kPresentTitle;
  presentItem.textColor = [UIColor colorNamed:kBlueColor];
  [model addSectionWithIdentifier:kSectionIdentifierAction];
  [model addItem:presentItem toSectionWithIdentifier:kSectionIdentifierAction];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      kItemTypeAnimation) {
    [self configureAnimationCell:cell];
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case kItemTypeAnimation:
      // A row can't present a menu on tap, so trigger the accessory button,
      // whose primary action is the animation menu.
      [_animationMenuButton performPrimaryAction];
      break;
    case kItemTypePresent:
      [self presentSelectedItem];
      break;
    case kItemTypeNavigationController:
      break;
  }
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  [self dismissPromo];
}

- (void)didTapSecondaryActionButton {
  // Previewed promos only have a primary action.
}

- (void)didTapTertiaryActionButton {
  // Previewed promos only have a primary action.
}

- (void)didDismissButtonStackViewController {
  // Nothing to clean up when the preview is swiped away.
}

#pragma mark - Private (Model)

// Updates the animation row with the selected animation.
- (void)updateAnimationItem {
  _animationItem.text = _selectedItem.animationName;
  _animationItem.detailText = _selectedItem.feature;
}

// Returns the item of the switch wrapping the promo in a navigation
// controller.
- (TableViewSwitchItem*)navigationControllerItem {
  TableViewSwitchItem* item =
      [[TableViewSwitchItem alloc] initWithType:kItemTypeNavigationController];
  item.text = kNavigationControllerSwitchTitle;
  item.on = _presentsInNavigationController;
  item.target = self;
  item.selector = @selector(navigationControllerSwitchToggled:);
  return item;
}

// Updates the presentation mode when the navigation controller switch is
// toggled. The cell is reconfigured so its configuration stays in sync with
// the switch: otherwise, re-applying the stale configuration (e.g. when the
// presented promo is dismissed) resets the switch to its previous value.
- (void)navigationControllerSwitchToggled:(UISwitch*)sender {
  _presentsInNavigationController = sender.isOn;
  _navigationControllerItem.on = sender.isOn;
  [self reconfigureCellsForItems:@[ _navigationControllerItem ]];
}

// Selects `item` and refreshes the animation row.
- (void)selectItem:(LottieCatalogItem*)item {
  _selectedItem = item;
  [self updateAnimationItem];
  [self reconfigureCellsForItems:@[ _animationItem ]];
}

#pragma mark - Private (Menu)

// Configures the animation row `cell`. The chevron accessory is a button
// carrying the animation menu, so the menu is anchored to it. Taps on the rest
// of the row trigger the button programmatically.
- (void)configureAnimationCell:(UITableViewCell*)cell {
  if (!_animationMenuButton) {
    _animationMenuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_animationMenuButton
        setImage:DefaultAccessorySymbolConfigurationWithRegularWeight(
                     SymbolChevronUpDown)
        forState:UIControlStateNormal];
    _animationMenuButton.menu = [self animationMenu];
    _animationMenuButton.showsMenuAsPrimaryAction = YES;
    [_animationMenuButton sizeToFit];
  }
  cell.accessoryView = _animationMenuButton;
}

// Returns a menu listing all the animations, with one submenu per feature.
// The menu is rebuilt each time it opens so the selected animation is checked.
- (UIMenu*)animationMenu {
  __weak __typeof(self) weakSelf = self;
  UIDeferredMenuElement* element =
      [UIDeferredMenuElement elementWithUncachedProvider:^(
                                 void (^completion)(NSArray<UIMenuElement*>*)) {
        completion([weakSelf featureMenus]);
      }];
  return [UIMenu menuWithChildren:@[ element ]];
}

// Returns one submenu per feature, in the order of `_items`.
- (NSArray<UIMenu*>*)featureMenus {
  NSMutableArray<UIMenu*>* menus = [NSMutableArray array];
  NSMutableArray<UIAction*>* actions = [NSMutableArray array];
  NSString* feature = _items.firstObject.feature;
  for (LottieCatalogItem* item in _items) {
    if (![item.feature isEqualToString:feature]) {
      [menus addObject:[self featureMenuWithTitle:feature children:actions]];
      actions = [NSMutableArray array];
      feature = item.feature;
    }
    [actions addObject:[self actionForItem:item]];
  }
  [menus addObject:[self featureMenuWithTitle:feature children:actions]];
  return menus;
}

// Returns a submenu titled `title` displaying `children`. Its subtitle shows
// the selected animation if it belongs to this feature.
- (UIMenu*)featureMenuWithTitle:(NSString*)title
                       children:(NSArray<UIMenuElement*>*)children {
  UIMenu* menu = [UIMenu menuWithTitle:title children:children];
  if ([_selectedItem.feature isEqualToString:title]) {
    menu.subtitle = _selectedItem.animationName;
  }
  return menu;
}

// Returns the menu action selecting `item`, checked if it is selected.
- (UIAction*)actionForItem:(LottieCatalogItem*)item {
  __weak __typeof(self) weakSelf = self;
  UIAction* action = [UIAction actionWithTitle:item.animationName
                                         image:nil
                                    identifier:nil
                                       handler:^(UIAction*) {
                                         [weakSelf selectItem:item];
                                       }];
  action.state =
      item == _selectedItem ? UIMenuElementStateOn : UIMenuElementStateOff;
  return action;
}

#pragma mark - Private (Presentation)

// Presents the selected animation, wrapped in a navigation controller if the
// switch is on.
- (void)presentSelectedItem {
  UIViewController* promo = [self promoWithBricksForItem:_selectedItem];
  if (!_presentsInNavigationController) {
    [self presentViewController:promo animated:YES completion:nil];
    return;
  }
  promo.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissPromo)];
  UINavigationController* navigationController =
      [[UINavigationController alloc] initWithRootViewController:promo];
  [self presentViewController:navigationController animated:YES completion:nil];
}

// Dismisses the presented promo.
- (void)dismissPromo {
  [self dismissViewControllerAnimated:YES completion:nil];
}

// Returns the animation configuration for `item`. The same file is used in
// light and dark mode so the exact file is previewed whatever the current
// interface style.
- (AnimationBrickConfiguration*)animationConfigurationForItem:
    (LottieCatalogItem*)item {
  AnimationBrickConfiguration* config =
      [[AnimationBrickConfiguration alloc] init];
  config.animationName = item.animationName;
  config.darkAnimationName = item.animationName;
  config.animationBackgroundColor = item.animationBackgroundColor;
  if (item.lightModeColorProvider || item.darkModeColorProvider) {
    config.useLegacyDarkMode = NO;
    config.lightModeColorProvider = item.lightModeColorProvider;
    config.darkModeColorProvider = item.darkModeColorProvider;
  }
  return config;
}

// Returns the title configuration for `item`, displaying its file name and its
// feature, so the previewed file can be identified.
- (TitleBrickConfiguration*)titleConfigurationForItem:(LottieCatalogItem*)item {
  TitleBrickConfiguration* config = [[TitleBrickConfiguration alloc] init];
  config.title = item.animationName;
  config.subtitle = item.feature;
  config.titleStyle = UIFontTextStyleTitle2;
  return config;
}

// Returns an `AnimatedPromoWithBricksViewController` previewing `item`.
- (AnimatedPromoWithBricksViewController*)promoWithBricksForItem:
    (LottieCatalogItem*)item {
  ButtonStackConfiguration* buttonConfig =
      [[ButtonStackConfiguration alloc] init];
  buttonConfig.primaryActionString = kCloseActionTitle;

  AnimatedPromoConfiguration* config = [[AnimatedPromoConfiguration alloc]
      initWithAnimationBrickConfiguration:
          [self animationConfigurationForItem:item]
                 buttonStackConfiguration:buttonConfig
                  titleBrickConfiguration:[self titleConfigurationForItem:item]
                           underTitleView:nil];
  AnimatedPromoWithBricksViewController* promo =
      [[AnimatedPromoWithBricksViewController alloc]
          initWithConfiguration:config];
  promo.actionDelegate = self;
  return promo;
}

@end
