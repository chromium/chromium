// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

NSString* const kActorAppInterfaceErrorDomain = @"ActorAppInterfaceErrorDomain";

const base::TimeDelta kApcFetchingTimeout = base::Seconds(10);

@implementation ActorAppInterface

+ (void)executeActionWithProto:(NSData*)actionProto
                    completion:(void (^)(NSError* error))completion {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  if (!profile) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolExecutionResultNoProfile
               userInfo:@{NSLocalizedDescriptionKey : @"No profile"}]);
    return;
  }

  actor::ActorService* service =
      actor::ActorServiceFactory::GetForProfile(profile);
  if (!service) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolExecutionResultNoService
               userInfo:@{NSLocalizedDescriptionKey : @"No service"}]);
    return;
  }

  optimization_guide::proto::Action action;
  if (!action.ParseFromArray([actionProto bytes], [actionProto length])) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolExecutionResultInvalidProto
               userInfo:@{NSLocalizedDescriptionKey : @"Invalid proto"}]);
    return;
  }

  actor::ActorTaskId task_id =
      service->CreateTask("EG Test Task", /*allow_incognito_web_states=*/false);

  std::vector<optimization_guide::proto::Action> actions = {action};
  actor::CreateActorToolRequestsResult tools_result =
      service->CreateActorToolRequests(actions, task_id);

  if (!tools_result.has_value()) {
    NSString* errorMsg = base::SysUTF8ToNSString(base::StringPrintf(
        "Failed to create tool requests: %s",
        actor::GetToolExecutionResultMessage(tools_result.error()).c_str()));
    NSError* error =
        [NSError errorWithDomain:@"mojom::ActionResultCode"
                            code:(NSInteger)tools_result.error().code()
                        userInfo:@{NSLocalizedDescriptionKey : errorMsg}];
    completion(error);
    return;
  }

  auto action_performed_callback =
      base::BindOnce(^(actor::PerformActionsResult result) {
        [ActorAppInterface handleActionResults:std::move(result.action_results)
                                    completion:completion];
      });

  service->PerformActions(task_id, std::move(tools_result.value()),
                          "Executing EG Test action",
                          std::move(action_performed_callback));
}

+ (void)handleActionResults:(std::vector<actor::ActionResult>)results
                 completion:(void (^)(NSError* error))completion {
  if (results.empty()) {
    NSError* error = [NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolExecutionResultNoActuationResults
               userInfo:@{
                 NSLocalizedDescriptionKey : @"No action results returned"
               }];
    completion(error);
    return;
  }

  const actor::ActionResult& result = results[0];
  if (result.tool_result.IsOk()) {
    completion(nil);
  } else {
    NSString* errorMsg = base::SysUTF8ToNSString(
        GetToolExecutionResultMessage(result.tool_result));
    NSError* error =
        [NSError errorWithDomain:@"mojom::ActionResultCode"
                            code:(NSInteger)result.tool_result.code()
                        userInfo:@{NSLocalizedDescriptionKey : errorMsg}];
    completion(error);
  }
}

+ (NSData*)fetchLatestAPC {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  if (!web_state) {
    return nil;
  }

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder()
          .SetUseRichExtraction(true)
          .SetUseRichExtractionWithActionable(true)
          .Build();

  __block NSData* resultData = nil;
  __block BOOL completed = NO;

  PageContextWrapper* wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state
                  config:config
      completionCallback:base::BindOnce(^(
                             PageContextWrapperCallbackResponse response) {
        if (response.has_value()) {
          std::string serialized;
          response.value()->SerializeToString(&serialized);
          resultData = [NSData dataWithBytes:serialized.data()
                                      length:serialized.length()];
        }
        completed = YES;
      })];
  wrapper.shouldGetAnnotatedPageContent = YES;
  [wrapper populatePageContextFieldsAsyncWithTimeout:kApcFetchingTimeout];

  bool success =
      base::test::ios::WaitUntilConditionOrTimeout(kApcFetchingTimeout, ^bool {
        return completed;
      });
  if (!success) {
    return nil;
  }
  return resultData;
}

+ (void)waitForPageStabilityWithCompletion:
    (void (^)(NSError* error))completion {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  if (!web_state) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolExecutionResultNoWebState
               userInfo:@{NSLocalizedDescriptionKey : @"No web state"}]);
    return;
  }

  web::WebFramesManager* frames_manager =
      actor::PageStabilityJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state);
  if (!frames_manager || !frames_manager->GetMainWebFrame()) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolExecutionResultNoMainFrame
               userInfo:@{NSLocalizedDescriptionKey : @"No main frame"}]);
    return;
  }
  web::WebFrame* main_frame = frames_manager->GetMainWebFrame();

  auto callback = base::BindOnce(^(actor::ToolExecutionResult result) {
    if (result.IsOk()) {
      completion(nil);
    } else {
      NSString* errorMsg =
          base::SysUTF8ToNSString(GetToolExecutionResultMessage(result));
      NSError* error =
          [NSError errorWithDomain:@"mojom::ActionResultCode"
                              code:(NSInteger)result.code()
                          userInfo:@{NSLocalizedDescriptionKey : errorMsg}];
      completion(error);
    }
  });

  actor::PageStabilityJavaScriptFeature::GetInstance()->WaitForStability(
      main_frame->AsWeakPtr(), std::move(callback));
}

@end
