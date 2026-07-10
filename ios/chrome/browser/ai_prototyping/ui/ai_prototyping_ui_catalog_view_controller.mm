// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_ui_catalog_view_controller.h"

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actor_tool_chip_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Cell identifier for the UI Catalog items.
NSString* const kUICatalogCellIdentifier = @"UICatalogCell";

}  // namespace

// Simple helper class representing a UI Catalog item.
@interface AIPrototypingUICatalogItem : NSObject

// Text display in the catalog entries.
@property(nonatomic, copy) NSString* title;
// The class object of the view controller to present.
@property(nonatomic, assign) Class viewControllerClass;

@end

@implementation AIPrototypingUICatalogItem
@end

@implementation AIPrototypingUICatalogViewController {
  NSArray<AIPrototypingUICatalogItem*>* _items;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"UI Catalog";
  self.view.backgroundColor = [UIColor colorNamed:kSolidWhiteColor];

  [self.tableView registerClass:[UITableViewCell class]
         forCellReuseIdentifier:kUICatalogCellIdentifier];

  AIPrototypingUICatalogItem* chipsItem =
      [[AIPrototypingUICatalogItem alloc] init];
  chipsItem.title = @"Actor Tool Chips";
  chipsItem.viewControllerClass =
      [AIPrototypingActorToolChipViewController class];

  _items = @[ chipsItem ];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _items.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kUICatalogCellIdentifier
                                      forIndexPath:indexPath];
  AIPrototypingUICatalogItem* item = _items[indexPath.row];
  cell.textLabel.text = item.title;
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  AIPrototypingUICatalogItem* item = _items[indexPath.row];
  if (item.viewControllerClass) {
    UIViewController* debugVC = [[item.viewControllerClass alloc] init];
    [self.navigationController pushViewController:debugVC animated:YES];
  }
}

@end

#pragma mark - AIPrototypingUICatalogNavigationController

@implementation AIPrototypingUICatalogNavigationController {
  AIPrototypingFeature _feature;
  __weak id<AIPrototypingMutator> _mutator;
}

@synthesize feature = _feature;
@synthesize mutator = _mutator;

- (instancetype)initForFeature:(AIPrototypingFeature)feature {
  AIPrototypingUICatalogViewController* catalogVC =
      [[AIPrototypingUICatalogViewController alloc] init];
  self = [super initWithRootViewController:catalogVC];
  if (self) {
    _feature = feature;
  }
  return self;
}

@end
