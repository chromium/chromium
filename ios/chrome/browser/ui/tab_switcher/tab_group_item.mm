// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_utils.h"

@implementation TabGroupItem {
  WebStateList* _webStateList;
  NSMutableArray<GroupTabInfo*>* _tabGroupInfos;
}

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup
                    webStateList:(WebStateList*)webStateList {
  CHECK(tabGroup);
  CHECK(webStateList);
  CHECK(webStateList->ContainsGroup(tabGroup));
  self = [super init];
  if (self) {
    _tabGroup = tabGroup;
    _webStateList = webStateList;
    _tabGroupInfos = [[NSMutableArray alloc] init];
  }
  return self;
}

- (NSString*)title {
  if (!_webStateList->ContainsGroup(_tabGroup)) {
    return nil;
  }
  return _tabGroup->GetTitle();
}

- (NSString*)rawTitle {
  if (!_webStateList->ContainsGroup(_tabGroup)) {
    return nil;
  }
  return _tabGroup->GetRawTitle();
}

- (UIColor*)groupColor {
  if (!_webStateList->ContainsGroup(_tabGroup)) {
    return nil;
  }
  return _tabGroup->GetColor();
}

- (NSInteger)numberOfTabsInGroup {
  if (!_webStateList->ContainsGroup(_tabGroup)) {
    return 0;
  }
  return _tabGroup->range().count();
}

- (BOOL)collapsed {
  if (!_webStateList->ContainsGroup(_tabGroup)) {
    return NO;
  }
  return _tabGroup->visual_data().is_collapsed();
}

- (void)fetchGroupTabInfos:(GroupTabInfosFetchingCompletionBlock)completion {
  if (!_webStateList->ContainsGroup(_tabGroup)) {
    __weak TabGroupItem* weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          completion(weakSelf, nil);
        }));
    return;
  }

  // Reset the array to ensure to not display old snapshots and or favicons.
  [_tabGroupInfos removeAllObjects];
  NSUInteger numberOfRequestedImages = 0;
  for (int index : _tabGroup->range()) {
    if (numberOfRequestedImages >= 7) {
      break;
    }
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    CHECK(webState);
    __weak TabGroupItem* weakSelf = self;
    [TabGroupUtils fetchTabGroupInfoFromWebState:webState
                                      completion:^(GroupTabInfo* info) {
                                        [weakSelf addInfo:info];
                                        [weakSelf notifyCompletion:completion];
                                      }];
    numberOfRequestedImages++;
  }
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"Group Title: %@", self.title];
}

#pragma mark - Private helpers

// Adds the given info to the GroupTabInfo array.
- (void)addInfo:(GroupTabInfo*)info {
  [_tabGroupInfos addObject:info];
}

// Saves the snapshot and favicon couple in the same GroupTabInfo. Call the
// completion if there is no new snapshot or favicon to save.

- (void)notifyCompletion:(GroupTabInfosFetchingCompletionBlock)completion {
  if (!_webStateList->ContainsGroup(_tabGroup)) {
    completion(self, @[]);
    return;
  }
  if (static_cast<int>([_tabGroupInfos count]) ==
      MIN(_tabGroup->range().count(), 7)) {
    completion(self, _tabGroupInfos);
  }
}

@end
