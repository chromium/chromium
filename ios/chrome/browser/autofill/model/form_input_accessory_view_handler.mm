// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_input_accessory_view_handler.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"

namespace {

NSString* const kFormSuggestionAssistButtonPreviousElement = @"previousTap";
NSString* const kFormSuggestionAssistButtonNextElement = @"nextTap";
NSString* const kFormSuggestionAssistButtonDone = @"done";

}  // namespace

namespace {

// Finds all views of a particular kind if class `aClass` in the subview
// hierarchy of the given `root` view.
NSArray* SubviewsWithClass(UIView* root, Class aClass) {
  DCHECK(root);
  NSMutableArray* viewsToExamine = [NSMutableArray arrayWithObject:root];
  NSMutableArray* subviews = [NSMutableArray array];

  while ([viewsToExamine count]) {
    UIView* view = [viewsToExamine lastObject];
    if ([view isKindOfClass:aClass])
      [subviews addObject:view];

    [viewsToExamine removeLastObject];
    [viewsToExamine addObjectsFromArray:[view subviews]];
  }

  return subviews;
}

// Returns true if `item`'s action name contains `actionName`.
BOOL ItemActionMatchesName(UIBarButtonItem* item, NSString* actionName) {
  SEL itemAction = [item action];
  if (!itemAction)
    return false;
  NSString* itemActionName = NSStringFromSelector(itemAction);

  // This doesn't do a strict string match for the action name.
  return [itemActionName rangeOfString:actionName].location != NSNotFound;
}

// Finds all UIToolbarItems associated with a given UIToolbar `toolbar` with
// action selectors with a name that contains the action name specified by
// `actionName`.
NSArray* FindToolbarItemsForActionName(UIToolbar* toolbar,
                                       NSString* actionName) {
  NSMutableArray* toolbarItems = [NSMutableArray array];

  for (UIBarButtonItem* item in [toolbar items]) {
    if (ItemActionMatchesName(item, actionName))
      [toolbarItems addObject:item];
  }

  return toolbarItems;
}

// Finds all UIToolbarItem(s) with action selectors of the name specified by
// `actionName` in any UIToolbars in the view hierarchy below `root`.
NSArray* FindDescendantToolbarItemsForActionName(UIView* root,
                                                 NSString* actionName) {
  NSMutableArray* descendants = [NSMutableArray array];

  NSArray* toolbars = SubviewsWithClass(root, [UIToolbar class]);
  for (UIToolbar* toolbar in toolbars) {
    [descendants
        addObjectsFromArray:FindToolbarItemsForActionName(toolbar, actionName)];
  }

  return descendants;
}

// Finds all UIBarButtonItem(s) with action selectors of the name specified by
// `actionName` in the UITextInputAssistantItem passed.
NSArray* FindDescendantToolbarItemsForActionName(
    UITextInputAssistantItem* inputAssistantItem,
    NSString* actionName) {
  NSMutableArray* toolbarItems = [NSMutableArray array];

  NSMutableArray* buttonGroupsGroup = [[NSMutableArray alloc] init];
  if (inputAssistantItem.leadingBarButtonGroups)
    [buttonGroupsGroup addObject:inputAssistantItem.leadingBarButtonGroups];
  if (inputAssistantItem.trailingBarButtonGroups)
    [buttonGroupsGroup addObject:inputAssistantItem.trailingBarButtonGroups];
  for (NSArray* buttonGroups in buttonGroupsGroup) {
    for (UIBarButtonItemGroup* group in buttonGroups) {
      NSArray* items = group.barButtonItems;
      for (UIBarButtonItem* item in items) {
        if (ItemActionMatchesName(item, actionName))
          [toolbarItems addObject:item];
      }
    }
  }

  return toolbarItems;
}

}  // namespace

@interface FormInputAccessoryViewHandler () {
  // The frameId of the frame containing the form with the latest focus.
  NSString* _lastFocusFormActivityWebFrameID;
}

// The frameId of the frame containing the form with the latest focus.
@property(nonatomic) NSString* lastFocusFormActivityWebFrameID;

@end

@implementation FormInputAccessoryViewHandler

@synthesize webState = _webState;

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)setLastFocusFormActivityWebFrameID:(NSString*)frameID {
  _lastFocusFormActivityWebFrameID = frameID;
}

// Attempts to execute/tap/send-an-event-to the iOS built-in "next" and
// "previous" form assist controls. Returns NO if this attempt failed, YES
// otherwise. [HACK] Because the buttons on the assist controls can change any
// time, this can break with any new iOS version.
- (BOOL)executeFormAssistAction:(NSString*)actionName {
  NSArray* descendants = nil;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    // There is no input accessory view for iPads, instead Apple adds the assist
    // controls to the UITextInputAssistantItem.
    UIResponder* firstResponder = GetFirstResponder();
    UITextInputAssistantItem* inputAssistantItem =
        firstResponder.inputAssistantItem;
    if (!inputAssistantItem)
      return NO;
    descendants =
        FindDescendantToolbarItemsForActionName(inputAssistantItem, actionName);
  } else {
    UIResponder* firstResponder = GetFirstResponder();
    UIView* inputAccessoryView = firstResponder.inputAccessoryView;
    if (!inputAccessoryView)
      return NO;
    descendants =
        FindDescendantToolbarItemsForActionName(inputAccessoryView, actionName);
  }

  if (![descendants count])
    return NO;

  UIBarButtonItem* item = descendants.firstObject;
  if (!item.enabled) {
    return NO;
  }
  if (![[item target] respondsToSelector:[item action]]) {
    return NO;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  @try {
    // In some cases the keyboard is causing an exception when dismissing. Note:
    // this also happens without interactions with the input accessory, here we
    // can only catch the exceptions initiated here.
    [[item target] performSelector:[item action] withObject:item];
  } @catch (NSException* exception) {
    NOTREACHED_IN_MIGRATION() << exception.debugDescription;
    return NO;
  }
#pragma clang diagnostic pop
  return YES;
}

#pragma mark - FormInputNavigator

- (void)closeKeyboardWithButtonPress {
  [self closeKeyboardLoggingButtonPressed];
}

- (void)closeKeyboardWithoutButtonPress {
  [self closeKeyboardLoggingButtonPressed];
}

- (void)closeKeyboardWithOmniboxTypingShield {
  if (_webState) {
    UIView* view = _webState->GetView();
    CHECK(view);
    [view endEditing:YES];
  }
}

- (void)selectPreviousElementWithButtonPress {
  [self selectPreviousElementLoggingButtonPressed];
}

- (void)selectPreviousElementWithoutButtonPress {
  [self selectPreviousElementLoggingButtonPressed];
}

- (void)selectNextElementWithButtonPress {
  [self selectNextElementLoggingButtonPressed];
}

- (void)selectNextElementWithoutButtonPress {
  [self selectNextElementLoggingButtonPressed];
}

- (void)fetchPreviousAndNextElementsPresenceWithCompletionHandler:
    (void (^)(bool, bool))completionHandler {
  DCHECK(completionHandler);

  if (!_webState || IsKeyboardAccessoryUpgradeEnabled()) {
    completionHandler(false, false);
    return;
  }

  web::WebFrame* frame = [self webFrame];

  if (!frame) {
    completionHandler(false, false);
    return;
  }

  autofill::SuggestionControllerJavaScriptFeature::GetInstance()
      ->FetchPreviousAndNextElementsPresenceInFrame(
          frame, base::BindOnce(completionHandler));
}

#pragma mark - Private

// Tries to close the keyboard sending an action to the default accessory bar.
// If that fails, fallbacks on the view to resign the first responder status.
- (void)closeKeyboardLoggingButtonPressed {
  NSString* actionName = kFormSuggestionAssistButtonDone;
  BOOL performedAction = [self executeFormAssistAction:actionName];

  if (!performedAction && _webState) {
    UIView* view = _webState->GetView();
    DCHECK(view);
    [view endEditing:YES];
  }
}

// Tries to focus on the next element sendind an action to the default accessory
// bar if that fails, fallbacks on JavaScript.
- (void)selectPreviousElementLoggingButtonPressed {
  NSString* actionName = kFormSuggestionAssistButtonPreviousElement;
  BOOL performedAction = [self executeFormAssistAction:actionName];

  if (!performedAction && _webState) {
    // We could not find the built-in form assist controls, so try to focus
    // the next or previous control using JavaScript.
    web::WebFrame* frame = [self webFrame];

    if (frame) {
      autofill::SuggestionControllerJavaScriptFeature::GetInstance()
          ->SelectPreviousElementInFrame(frame);
    }
  }
}

// Tries to focus on the previous element sendind an action to the default
// accessory bar if that fails, fallbacks on JavaScript.
- (void)selectNextElementLoggingButtonPressed {
  NSString* actionName = kFormSuggestionAssistButtonNextElement;
  BOOL performedAction = [self executeFormAssistAction:actionName];

  if (!performedAction && _webState) {
    // We could not find the built-in form assist controls, so try to focus
    // the next or previous control using JavaScript.
    web::WebFrame* frame = [self webFrame];

    if (frame) {
      autofill::SuggestionControllerJavaScriptFeature::GetInstance()
          ->SelectNextElementInFrame(frame);
    }
  }
}

// Attempts to fetch the frame object from its id. May return nil.
- (web::WebFrame*)webFrame {
  web::WebFramesManager* framesManager =
      autofill::SuggestionControllerJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(_webState);
  return framesManager->GetFrameWithId(
      base::SysNSStringToUTF8(_lastFocusFormActivityWebFrameID));
}

@end
