// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

NSString* GetIdForWebState(web::WebState* web_state) {
  TabIdTabHelper::CreateForWebState(web_state);
  return TabIdTabHelper::FromWebState(web_state)->tab_id();
}

WebStateList* GetCurrentWebStateList() {
  return chrome_test_util::GetMainController()
      .interfaceProvider.currentInterface.tabModel.webStateList;
}

web::WebState* GetWebStateWithId(NSString* tab_id) {
  WebStateList* web_state_list = GetCurrentWebStateList();
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if ([tab_id isEqualToString:GetIdForWebState(web_state)])
      return web_state;
  }
  return nil;
}

// Returns the index of the WebState with the given tab_id, or
// WebStateList::kInvalidIndex if no such WebState is found.
int GetIndexOfWebStateWithId(NSString* tab_id) {
  WebStateList* web_state_list = GetCurrentWebStateList();
  return web_state_list->GetIndexOfWebState(GetWebStateWithId(tab_id));
}

}  // namespace

@implementation CWTWebDriverAppInterface

+ (NSError*)loadURL:(NSString*)URL
               inTab:(NSString*)tabID
    timeoutInSeconds:(NSTimeInterval)timeout {
  __block web::WebState* webState = nullptr;
  grey_dispatch_sync_on_main_thread(^{
    webState = GetWebStateWithId(tabID);
    if (webState)
      web::test::LoadUrl(webState, GURL(base::SysNSStringToUTF8(URL)));
  });

  if (!webState)
    return testing::NSErrorWithLocalizedDescription(@"No matching tab");

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL isLoading = NO;
    grey_dispatch_sync_on_main_thread(^{
      isLoading = webState->IsLoading();
    });
    return !isLoading;
  });

  if (success)
    return nil;

  return testing::NSErrorWithLocalizedDescription(@"Page load timed out");
}

+ (NSString*)currentTabID {
  __block NSString* tabID = nil;
  grey_dispatch_sync_on_main_thread(^{
    web::WebState* webState = chrome_test_util::GetCurrentWebState();
    if (webState)
      tabID = GetIdForWebState(webState);
  });

  return tabID;
}

+ (NSArray*)tabIDs {
  __block NSMutableArray* tabIDs;
  grey_dispatch_sync_on_main_thread(^{
    DCHECK(!chrome_test_util::IsIncognitoMode());
    WebStateList* webStateList = GetCurrentWebStateList();
    tabIDs = [NSMutableArray arrayWithCapacity:webStateList->count()];

    for (int i = 0; i < webStateList->count(); ++i) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      [tabIDs addObject:GetIdForWebState(webState)];
    }
  });

  return tabIDs;
}

+ (NSError*)closeTabWithID:(NSString*)ID {
  __block NSError* error = nil;
  grey_dispatch_sync_on_main_thread(^{
    int webStateIndex = GetIndexOfWebStateWithId(ID);
    if (webStateIndex != WebStateList::kInvalidIndex) {
      WebStateList* webStateList = GetCurrentWebStateList();
      webStateList->CloseWebStateAt(webStateIndex,
                                    WebStateList::CLOSE_USER_ACTION);
    } else {
      error = testing::NSErrorWithLocalizedDescription(@"No matching tab");
    }
  });

  return error;
}

+ (NSError*)switchToTabWithID:(NSString*)ID {
  __block NSError* error = nil;
  grey_dispatch_sync_on_main_thread(^{
    DCHECK(!chrome_test_util::IsIncognitoMode());
    int webStateIndex = GetIndexOfWebStateWithId(ID);
    if (webStateIndex != WebStateList::kInvalidIndex) {
      WebStateList* webStateList = GetCurrentWebStateList();
      webStateList->ActivateWebStateAt(webStateIndex);
    } else {
      error = testing::NSErrorWithLocalizedDescription(@"No matching tab");
    }
  });

  return error;
}

+ (NSString*)executeAsyncJavaScriptFunction:(NSString*)function
                                      inTab:(NSString*)tabID
                           timeoutInSeconds:(NSTimeInterval)timeout {
  const std::string kMessageResultKey("result");

  // Use a distinct messageID value for each invocation of this method to
  // distinguish stale messages (from previous script invocations that timed
  // out) from a message for the current script.
  static NSUInteger messageID = 0;
  std::string command = base::StringPrintf("CWTWebDriver%lu", messageID++);

  // Construct a completion handler that takes a single argument and sends a
  // message with this argument.
  std::string scriptCompletionHandler =
      base::StringPrintf("function(value) {"
                         "__gCrWeb.message.invokeOnHost({command: "
                         "'%s.result', %s: value}); }",
                         command.c_str(), kMessageResultKey.c_str());

  // Construct a script that calls the given |function| with
  // |scriptCompletionHandler| as an argument.
  std::string scriptFunctionWithCompletionHandler = base::StringPrintf(
      "(%s).call(null, %s)", base::SysNSStringToUTF8(function).c_str(),
      scriptCompletionHandler.c_str());

  __block base::Optional<base::Value> messageValue;
  const web::WebState::ScriptCommandCallback callback =
      base::BindRepeating(^(const base::DictionaryValue& value, const GURL&,
                            /*interacted*/ bool,
                            /*sender_frame*/ web::WebFrame*) {
        const base::Value* result = value.FindKey(kMessageResultKey);

        // |result| will be null when the computed result in JavaScript is
        // |undefined|. This happens, for example, when injecting a script that
        // performs some action (like setting the document's title) but doesn't
        // return any value.
        if (result)
          messageValue = result->Clone();
        else
          messageValue = base::Value();
      });

  __block BOOL webStateFound = NO;
  __block std::unique_ptr<web::WebState::ScriptCommandSubscription>
      subscription;
  grey_dispatch_sync_on_main_thread(^{
    web::WebState* webState = GetWebStateWithId(tabID);
    if (!webState)
      return;
    webStateFound = YES;
    subscription = webState->AddScriptCommandCallback(callback, command);
    webState->ExecuteJavaScript(
        base::UTF8ToUTF16(scriptFunctionWithCompletionHandler));
  });

  if (!webStateFound)
    return nil;

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL scriptExecutionComplete = NO;
    grey_dispatch_sync_on_main_thread(^{
      scriptExecutionComplete = messageValue.has_value();
    });
    return scriptExecutionComplete;
  });

  if (!success)
    return nil;

  std::string resultAsJSON;
  base::JSONWriter::Write(*messageValue, &resultAsJSON);
  return base::SysUTF8ToNSString(resultAsJSON);
}

+ (void)enablePopups {
  grey_dispatch_sync_on_main_thread(^{
    chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
  });
}

+ (NSString*)takeSnapshotOfTabWithID:(NSString*)ID {
  __block web::WebState* webState;
  grey_dispatch_sync_on_main_thread(^{
    webState = GetWebStateWithId(ID);
  });

  if (!webState)
    return nil;

  __block UIImage* snapshot = nil;
  grey_dispatch_sync_on_main_thread(^{
    CGRect bounds = webState->GetWebViewProxy().bounds;
    UIEdgeInsets insets = webState->GetWebViewProxy().contentInset;
    CGRect adjustedBounds = UIEdgeInsetsInsetRect(bounds, insets);

    webState->TakeSnapshot(gfx::RectF(adjustedBounds),
                           base::BindRepeating(^(const gfx::Image& image) {
                             snapshot = image.ToUIImage();
                           }));
  });

  const NSTimeInterval kSnapshotTimeoutSeconds = 100;
  bool success = WaitUntilConditionOrTimeout(kSnapshotTimeoutSeconds, ^bool {
    __block BOOL snapshotComplete = NO;
    grey_dispatch_sync_on_main_thread(^{
      if (snapshot != nil)
        snapshotComplete = YES;
    });
    return snapshotComplete;
  });

  if (!success)
    return nil;

  NSData* snapshotAsPNG = UIImagePNGRepresentation(snapshot);
  return [snapshotAsPNG base64EncodedStringWithOptions:0];
}

@end
