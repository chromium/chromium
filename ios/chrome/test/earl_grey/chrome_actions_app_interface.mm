// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/element_selector.h"

namespace {

// Action to swipe left on 150pt.
NSArray<NSValue*>* SwipeLeft(CGPoint startPoint) {
  const CGFloat total_length = 150;
  const int number_of_frames = 30;
  const CGFloat deltaX = total_length / number_of_frames;
  // Initial displacement to trigger a swipe.
  const int initial_displacement = 10;
  const int beginning = ceil(initial_displacement / deltaX);

  NSMutableArray* touchPath = [[NSMutableArray alloc] init];
  [touchPath addObject:[NSValue valueWithCGPoint:startPoint]];

  for (int i = beginning; i < number_of_frames; i++) {
    CGPoint point = CGPointMake(startPoint.x - i * deltaX, startPoint.y);
    [touchPath addObject:[NSValue valueWithCGPoint:point]];
  }

  [touchPath addObject:[NSValue valueWithCGPoint:CGPointMake(startPoint.x -
                                                                 total_length,
                                                             startPoint.y)]];

  return touchPath;
}

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
                base::apple::ObjCCast<TableViewSwitchCell>(collectionViewCell);
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
              id<GREYAction> action = [GREYActions actionForTurnSwitchOn:on];
              success = [action perform:switchView error:errorOrNil];
              return;
            }
            success = YES;
          });
          return success;
        }];
}

+ (id<GREYAction>)tapWebElement:(ElementSelector*)selector {
  return web::WebViewTapElement(chrome_test_util::GetCurrentWebState(),
                                selector, /*verified*/ true);
}

+ (id<GREYAction>)tapWebElementUnverified:(ElementSelector*)selector {
  return web::WebViewTapElement(chrome_test_util::GetCurrentWebState(),
                                selector, /*verified*/ false);
}

+ (id<GREYAction>)longPressOnHiddenElement {
  GREYPerformBlock longPress = ^BOOL(id element, __strong NSError** error) {
    grey_dispatch_sync_on_main_thread(^{
      UIView* view = base::apple::ObjCCast<UIView>(element);
      if (!view) {
        *error = [NSError
            errorWithDomain:kChromeActionsErrorDomain
                       code:0
                   userInfo:@{
                     NSLocalizedDescriptionKey : @"View is not a UIView"
                   }];
        return;
      }

      [GREYTapper longPressOnElement:element
                            location:view.center
                            duration:0.7
                               error:error];
    });
    return YES;
  };
  return [GREYActionBlock actionWithName:@"Long press on hidden element"
                            performBlock:longPress];
}

+ (id<GREYAction>)scrollToTop {
  GREYPerformBlock scrollToTopBlock = ^BOOL(id element,
                                            __strong NSError** error) {
    grey_dispatch_sync_on_main_thread(^{
      UIScrollView* view = base::apple::ObjCCast<UIScrollView>(element);
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

+ (id<GREYAction>)swipeToShowDeleteButton {
  return [GREYActionBlock
      actionWithName:@"Swipe to display delete button"
         constraints:nil
        performBlock:^(UIView* element, NSError* __strong* errorOrNil) {
          if ([element window] == nil) {
            NSString* errorDescription = [NSString
                stringWithFormat:
                    @"Cannot swipe on this view as it has no window and "
                    @"isn't a window itself:\n%@",
                    [element grey_description]];
            *errorOrNil = [NSError
                errorWithDomain:@"No window available"
                           code:0
                       userInfo:@{@"Failure Reason" : (errorDescription)}];
            // Indicates that the action failed.
            return NO;
          }
          CGRect accessibilityFrame = element.accessibilityFrame;
          CGPoint startPoint = CGPointMake(
              accessibilityFrame.origin.x + accessibilityFrame.size.width * 0.5,
              accessibilityFrame.origin.y +
                  accessibilityFrame.size.height * 0.5);
          // Invoke a custom selector that animates the window of the element.
          [GREYSyntheticEvents touchAlongPath:SwipeLeft(startPoint)
                             relativeToWindow:[element window]
                                  forDuration:1
                                      timeout:10];
          // Indicates that the action was executed successfully.
          return YES;
        }];
}

+ (id<GREYAction>)accessibilitySwipeRight {
  return [GREYActionBlock
      actionWithName:@"Swipe right with 3-finger"
         constraints:nil
        performBlock:^(UIScrollView* element, NSError* __strong* errorOrNil) {
          if (![element isKindOfClass:UIScrollView.class]) {
            NSString* errorDescription =
                [NSString stringWithFormat:@"Cannot swipe on this view as it "
                                           @"is not a scroll view:\n%@",
                                           [element grey_description]];
            *errorOrNil = [NSError
                errorWithDomain:@"Not a scroll view"
                           code:0
                       userInfo:@{@"Failure Reason" : (errorDescription)}];
            // Indicates that the action failed.
            return NO;
          }
          if ([element window] == nil) {
            NSString* errorDescription = [NSString
                stringWithFormat:
                    @"Cannot swipe on this view as it has no window and "
                    @"isn't a window itself:\n%@",
                    [element grey_description]];
            *errorOrNil = [NSError
                errorWithDomain:@"No window available"
                           code:0
                       userInfo:@{@"Failure Reason" : (errorDescription)}];
            // Indicates that the action failed.
            return NO;
          }
          CGPoint currentOffset = element.contentOffset;
          currentOffset.x = currentOffset.x - element.bounds.size.width;
          [element setContentOffset:currentOffset animated:NO];
          [element.delegate scrollViewDidEndDecelerating:element];
          // Indicates that the action was executed successfully.
          return YES;
        }];
}

@end
