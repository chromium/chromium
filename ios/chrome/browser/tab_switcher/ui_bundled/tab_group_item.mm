// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/color_palette/tab_group_color_palette.h"

@implementation TabGroupItem {
  base::WeakPtr<const TabGroup> _tabGroup;
  raw_ptr<const void, DanglingUntriaged> _tabGroupIdentifier;
  TabGroupColorPalette* _tabGroupColorPalette;
}

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup {
  CHECK(tabGroup);
  self = [super init];
  if (self) {
    _tabGroup = tabGroup->GetWeakPtr();
    _tabGroupIdentifier = tabGroup;
  }
  return self;
}

- (const void*)tabGroupIdentifier {
  return _tabGroupIdentifier;
}

- (const TabGroup*)tabGroup {
  return _tabGroup.get();
}

- (NSString*)title {
  if (!_tabGroup) {
    return nil;
  }
  return _tabGroup->GetTitle();
}

- (UIColor*)groupColor {
  CHECK(!IsTabGroupColorOnSurfaceEnabled());
  if (!_tabGroup) {
    return nil;
  }
  return tab_groups::ColorForTabGroupColorId(_tabGroup->GetColor());
}

- (TabGroupColorPalette*)tabGroupColorPalette {
  CHECK(IsTabGroupColorOnSurfaceEnabled());
  if (!_tabGroup) {
    return nil;
  }
  if (!_tabGroupColorPalette) {
    _tabGroupColorPalette = [[TabGroupColorPalette alloc]
        initWithSeedColorId:_tabGroup->GetColor()];
  }
  return _tabGroupColorPalette;
}

- (UIColor*)foregroundColor {
  if (!_tabGroup) {
    return nil;
  }
  return tab_groups::ForegroundColorForTabGroupColorId(_tabGroup->GetColor());
}

- (NSInteger)numberOfTabsInGroup {
  if (!_tabGroup) {
    return 0;
  }
  return _tabGroup->range().count();
}

- (BOOL)collapsed {
  if (!_tabGroup) {
    return NO;
  }
  return _tabGroup->visual_data().is_collapsed();
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"Group Title: %@", self.title];
}

@end
