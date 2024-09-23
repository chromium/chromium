// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"

#import <signal.h>

#import "base/files/file.h"
#import "base/files/file_path.h"
#import "base/json/json_writer.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/wpt/cwt_stderr_logger.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

NSString* GetIdForWebState(web::WebState* web_state) {
  return web_state->GetStableIdentifier();
}

WebStateList* GetCurrentWebStateList() {
  return chrome_test_util::GetForegroundActiveScene()
      .browserProviderInterface.currentBrowserProvider.browser
      ->GetWebStateList();
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

void DispatchSyncOnMainThread(void (^block)(void)) {
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_semaphore_t waitForBlock = dispatch_semaphore_create(0);
    CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopDefaultMode, ^{
      block();
      dispatch_semaphore_signal(waitForBlock);
    });
    // CFRunLoopPerformBlock does not wake up the main queue.
    CFRunLoopWakeUp(CFRunLoopGetMain());
    // Waits until block is executed and semaphore is signalled.
    dispatch_semaphore_wait(waitForBlock, DISPATCH_TIME_FOREVER);
  }
}

}  // namespace

@implementation CWTWebDriverAppInterface

- (instancetype)init {
  self = [super init];
  if (self) {
    _executingQueue = dispatch_queue_create("com.google.chrome.cwt.background",
                                            DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

+ (NSError*)loadURL:(NSString*)URL
              inTab:(NSString*)tabID
            timeout:(base::TimeDelta)timeout {
  __block web::WebState* webState = nullptr;
  DispatchSyncOnMainThread(^{
    webState = GetWebStateWithId(tabID);
    if (webState)
      web::test::LoadUrl(webState, GURL(base::SysNSStringToUTF8(URL)));
  });

  if (!webState)
    return testing::NSErrorWithLocalizedDescription(@"No matching tab");

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL isLoading = NO;
    DispatchSyncOnMainThread(^{
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
  DispatchSyncOnMainThread(^{
    web::WebState* webState = chrome_test_util::GetCurrentWebState();
    if (webState)
      tabID = GetIdForWebState(webState);
  });

  return tabID;
}

+ (NSArray*)tabIDs {
  __block NSMutableArray* tabIDs;
  DispatchSyncOnMainThread(^{
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
  DispatchSyncOnMainThread(^{
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

+ (NSString*)openNewTab {
  __block NSString* tabID = nil;
  DispatchSyncOnMainThread(^{
    chrome_test_util::OpenNewTab();
    tabID = GetIdForWebState(chrome_test_util::GetCurrentWebState());
  });

  return tabID;
}

+ (NSError*)switchToTabWithID:(NSString*)ID {
  __block NSError* error = nil;
  DispatchSyncOnMainThread(^{
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
                                    timeout:(base::TimeDelta)timeout {
  __block BOOL webStateFound = NO;
  __block std::optional<base::Value> messageValue;
  DispatchSyncOnMainThread(^{
    web::WebState* webState = GetWebStateWithId(tabID);
    if (!webState)
      return;
    web::WebFrame* mainFrame =
        webState->GetPageWorldWebFramesManager()->GetMainWebFrame();
    if (!mainFrame) {
      return;
    }
    webStateFound = YES;

    NSString* script =
        [NSString stringWithFormat:@"var result;"
                                   @"(%@).call(null, (r) => { result = r; } );"
                                   @"result;",
                                   function];

    mainFrame->ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                 base::BindOnce(^(const base::Value* result) {
                                   // `result` will be null when the computed
                                   // result in JavaScript is `undefined`. This
                                   // happens, for example, when injecting a
                                   // script that performs some action (like
                                   // setting the document's title) but doesn't
                                   // return any value.
                                   if (result) {
                                     messageValue = result->Clone();
                                   } else {
                                     messageValue = base::Value();
                                   }
                                 }));
  });

  if (!webStateFound)
    return nil;

  bool success = WaitUntilConditionOrTimeout(timeout, ^bool {
    __block BOOL scriptExecutionComplete = NO;
    DispatchSyncOnMainThread(^{
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
  DispatchSyncOnMainThread(^{
    chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
  });
}

+ (NSString*)takeSnapshotOfTabWithID:(NSString*)ID {
  __block web::WebState* webState;
  DispatchSyncOnMainThread(^{
    webState = GetWebStateWithId(ID);
  });

  if (!webState)
    return nil;

  __block UIImage* snapshot = nil;
  DispatchSyncOnMainThread(^{
    CGRect bounds = webState->GetWebViewProxy().bounds;
    UIEdgeInsets insets = webState->GetWebViewProxy().contentInset;
    CGRect adjustedBounds = UIEdgeInsetsInsetRect(bounds, insets);

    webState->TakeSnapshot(adjustedBounds,
                           base::BindRepeating(^(UIImage* image) {
                             snapshot = image;
                           }));
  });

  constexpr base::TimeDelta kSnapshotTimeout = base::Seconds(100);
  bool success = WaitUntilConditionOrTimeout(kSnapshotTimeout, ^bool {
    __block BOOL snapshotComplete = NO;
    DispatchSyncOnMainThread(^{
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

+ (void)logStderrToFilePath:(NSString*)filePath {
  base::FilePath stderrPath(base::SysNSStringToUTF8(filePath));
  CWTStderrLogger::GetInstance()->StartRedirectingToFile(stderrPath);
}

+ (void)stopLoggingStderr {
  CWTStderrLogger::GetInstance()->StopRedirectingToFile();
}

+ (void)installCleanExitHandlerForAbortSignal {
  struct sigaction sa {};
  sa.sa_handler = [](int) { exit(0); };
  sigaction(SIGABRT, &sa, nullptr);
}

@end
