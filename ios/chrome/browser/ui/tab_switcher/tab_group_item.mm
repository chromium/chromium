// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_utils.h"

@implementation TabGroupItem {
  WebStateList* _webStateList;
  NSMutableDictionary<NSNumber*, GroupTabInfo*>* _tabGroupInfos;
  base::WeakPtr<const TabGroup> _tabGroup;
  const void* _tabGroupIdentifier;
}

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup
                    webStateList:(WebStateList*)webStateList {
  CHECK(tabGroup);
  CHECK(webStateList);
  CHECK(webStateList->ContainsGroup(tabGroup));
  self = [super init];
  if (self) {
    _tabGroup = tabGroup->GetWeakPtr();
    _tabGroupIdentifier = tabGroup;
    _webStateList = webStateList;
    _tabGroupInfos = [[NSMutableDictionary alloc] init];
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

- (NSString*)rawTitle {
  if (!_tabGroup) {
    return nil;
  }
  return _tabGroup->GetRawTitle();
}

- (UIColor*)groupColor {
  if (!_tabGroup) {
    return nil;
  }
  return _tabGroup->GetColor();
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

- (void)fetchGroupTabInfos:(GroupTabInfosFetchingCompletionBlock)completion {
  if (!_tabGroup) {
    __weak TabGroupItem* weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          completion(weakSelf, nil);
        }));
    return;
  }

  // Reset the array to ensure to not display old snapshots and or favicons.
  [_tabGroupInfos removeAllObjects];
  NSUInteger numberOfRequests = MIN(7, _tabGroup->range().count());
  int firstIndex = _tabGroup->range().range_begin();
  for (NSUInteger requestIndex = 0; requestIndex < numberOfRequests;
       requestIndex++) {
    web::WebState* webState =
        _webStateList->GetWebStateAt(firstIndex + requestIndex);
    CHECK(webState);
    __weak TabGroupItem* weakSelf = self;
    [TabGroupUtils fetchTabGroupInfoFromWebState:webState
                                      completion:^(GroupTabInfo* info) {
                                        [weakSelf addInfo:info
                                            forRequestNumber:requestIndex];
                                        [weakSelf
                                            notifyCompletion:completion
                                            numberOfRequests:numberOfRequests];
                                      }];
  }
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"Group Title: %@", self.title];
}

#pragma mark - Private helpers

// Adds the given info to the GroupTabInfo array.
- (void)addInfo:(GroupTabInfo*)info forRequestNumber:(NSUInteger)requestNumber {
  _tabGroupInfos[@(requestNumber)] = info;
}

// Saves the snapshot and favicon couple in the same GroupTabInfo. Call the
// completion if there is no new snapshot or favicon to save.
- (void)notifyCompletion:(GroupTabInfosFetchingCompletionBlock)completion
        numberOfRequests:(NSUInteger)numberOfRequests {
  if (!_tabGroup) {
    completion(self, @[]);
    return;
  }
  if (_tabGroupInfos.count == numberOfRequests) {
    auto comparator = ^NSComparisonResult(NSNumber* obj1, NSNumber* obj2) {
      return [obj1 compare:obj2];
    };
    NSMutableArray* infos = [NSMutableArray array];
    for (NSNumber* key in
         [[_tabGroupInfos allKeys] sortedArrayUsingComparator:comparator]) {
      [infos addObject:_tabGroupInfos[key]];
    }
    completion(self, infos);
  }
}

@end
