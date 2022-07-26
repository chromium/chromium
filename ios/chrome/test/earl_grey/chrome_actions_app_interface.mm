// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/element_selector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kChromeActionsErrorDomain = @"ChromeActionsError";
}  // namespace

@implementation ChromeActionsAppInterface : NSObject

+ (id<GREYAction>)longPressElement:(ElementSelector*)selector
                triggerContextMenu:(BOOL)triggerContextMenu {
  return WebViewLongPressElementForContextMenu(
      chrome_test_util::GetCurrentWebState(), selector, triggerContextMenu);
}

+ (id<GREYAction>)scrollElementToVisible:(ElementSelector*)selector {
  return WebViewScrollElementToVisible(chrome_test_util::GetCurrentWebState(),
                                       selector);
}

+ (id<GREYAction>)turnTableViewSwitchOn:(BOOL)on {
  id<GREYMatcher> constraints = grey_not(grey_systemAlertViewShown());
  NSString* actionName =
      [NSString stringWithFormat:@"Turn table view switch to %@ state",
                                 on ? @"ON" : @"OFF"];
  return [GREYActionBlock
      actionWithName:actionName
         constraints:constraints
        performBlock:^BOOL(id collectionViewCell,
                           __strong NSError** errorOrNil) {
          // EG2 executes actions on a background thread by default. Since this
          // action interacts with UI, kick it over to the main thread.
          __block BOOL success = NO;
          grey_dispatch_sync_on_main_thread(^{
            TableViewSwitchCell* switchCell =
                base::mac::ObjCCast<TableViewSwitchCell>(collectionViewCell);
            if (!switchCell) {
              NSString* description = @"The element isn't of the expected type "
                                      @"(TableViewSwitchCell).";
              *errorOrNil = [NSError
                  errorWithDomain:kChromeActionsErrorDomain
                             code:0
                         userInfo:@{NSLocalizedDescriptionKey : description}];
              success = NO;
              return;
            }
            UISwitch* switchView = switchCell.switchView;
            if (switchView.on != on) {
              id<GREYAction> longPressAction = [GREYActions
                  actionForLongPressWithDuration:kGREYLongPressDefaultDuration];
              success = [longPressAction perform:switchView error:errorOrNil];
              return;
            }
            success = YES;
          });
          return success;
        }];
}

+ (id<GREYAction>)turnSyncSwitchOn:(BOOL)on {
  id<GREYMatcher> constraints = grey_not(grey_systemAlertViewShown());
  NSString* actionName = [NSString
      stringWithFormat:@"Turn sync switch to %@ state", on ? @"ON" : @"OFF"];
  return [GREYActionBlock
      actionWithName:actionName
         constraints:constraints
        performBlock:^BOOL(id syncSwitchCell, __strong NSError** errorOrNil) {
          // EG2 executes actions on a background thread by default. Since this
          // action interacts with UI, kick it over to the main thread.
          __block BOOL success = NO;
          grey_dispatch_sync_on_main_thread(^{
            TableViewSwitchCell* switchCell =
                base::mac::ObjCCastStrict<TableViewSwitchCell>(syncSwitchCell);
            UISwitch* switchView = switchCell.switchView;
            if (switchView.on != on) {
              id<GREYAction> longPressAction = [GREYActions
                  actionForLongPressWithDuration:kGREYLongPressDefaultDuration];
              success = [longPressAction perform:switchView error:errorOrNil];
              return;
            }
            success = YES;
          });
          return success;
        }];
}

+ (id<GREYAction>)tapWebElement:(ElementSelector*)selector {
  return web::WebViewTapElement(chrome_test_util::GetCurrentWebState(),
                                selector);
}

+ (id<GREYAction>)scrollToTop {
  GREYPerformBlock scrollToTopBlock = ^BOOL(id element,
                                            __strong NSError** error) {
    grey_dispatch_sync_on_main_thread(^{
      UIScrollView* view = base::mac::ObjCCast<UIScrollView>(element);
      if (!view) {
        *error = [NSError
            errorWithDomain:kChromeActionsErrorDomain
                       code:0
                   userInfo:@{
                     NSLocalizedDescriptionKey : @"View is not a UIScrollView"
                   }];
      }
      view.contentOffset = CGPointZero;
    });
    return YES;
  };
  return [GREYActionBlock actionWithName:@"Scroll to top"
                            performBlock:scrollToTopBlock];
}

+ (id<GREYAction>)tapAtPointAtxOriginStartPercentage:(CGFloat)x
                              yOriginStartPercentage:(CGFloat)y {
  DCHECK(0 <= x && x <= 1);
  DCHECK(0 <= y && y <= 1);

  id<GREYMatcher> constraints = grey_notNil();
  NSString* actionName =
      [NSString stringWithFormat:@"Tap at point at percentage"];

  GREYPerformBlock actionBlock = ^BOOL(id view, __strong NSError** errorOrNil) {
    __block BOOL success = NO;
    grey_dispatch_sync_on_main_thread(^{
      CGRect rect = [view accessibilityFrame];
      CGPoint pointToTap =
          CGPointMake(rect.size.width * x, rect.size.height * y);
      success =
          [[GREYActions actionForTapAtPoint:pointToTap] perform:view
                                                          error:errorOrNil];
    });
    return success;
  };
  return [GREYActionBlock actionWithName:actionName
                             constraints:constraints
                            performBlock:actionBlock];
}

@end
