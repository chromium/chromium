// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_actions.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

id<GREYAction> LongPressElementForContextMenu(
    web::test::ElementSelector selector,
    bool triggers_context_menu) {
  return WebViewLongPressElementForContextMenu(
      chrome_test_util::GetCurrentWebState(), std::move(selector),
      triggers_context_menu);
}

id<GREYAction> TurnSettingsSwitchOn(BOOL on) {
  id<GREYMatcher> constraints = grey_not(grey_systemAlertViewShown());
  NSString* actionName =
      [NSString stringWithFormat:@"Turn settings switch to %@ state",
                                 on ? @"ON" : @"OFF"];
  return [GREYActionBlock
      actionWithName:actionName
         constraints:constraints
        performBlock:^BOOL(id collectionViewCell,
                           __strong NSError** errorOrNil) {
          LegacySettingsSwitchCell* switchCell =
              base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(
                  collectionViewCell);
          UISwitch* switchView = switchCell.switchView;
          if (switchView.on != on) {
            id<GREYAction> longPressAction = [GREYActions
                actionForLongPressWithDuration:kGREYLongPressDefaultDuration];
            return [longPressAction perform:switchView error:errorOrNil];
          }
          return YES;
        }];
}

id<GREYAction> TurnSyncSwitchOn(BOOL on) {
  id<GREYMatcher> constraints = grey_not(grey_systemAlertViewShown());
  NSString* actionName = [NSString
      stringWithFormat:@"Turn sync switch to %@ state", on ? @"ON" : @"OFF"];
  return [GREYActionBlock
      actionWithName:actionName
         constraints:constraints
        performBlock:^BOOL(id syncSwitchCell, __strong NSError** errorOrNil) {
          SyncSwitchCell* switchCell =
              base::mac::ObjCCastStrict<SyncSwitchCell>(syncSwitchCell);
          UISwitch* switchView = switchCell.switchView;
          if (switchView.on != on) {
            id<GREYAction> longPressAction = [GREYActions
                actionForLongPressWithDuration:kGREYLongPressDefaultDuration];
            return [longPressAction perform:switchView error:errorOrNil];
          }
          return YES;
        }];
}

id<GREYAction> TapWebElement(const std::string& element_id) {
  return web::WebViewTapElement(
      chrome_test_util::GetCurrentWebState(),
      web::test::ElementSelector::ElementSelectorId(element_id));
}

}  // namespace chrome_test_util
