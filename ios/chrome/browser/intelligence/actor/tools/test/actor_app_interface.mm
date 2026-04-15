// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/test/actor_app_interface.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"

NSString* const kActorAppInterfaceErrorDomain = @"ActorAppInterfaceErrorDomain";

const base::TimeDelta kApcFetchingTimeout = base::Seconds(10);

@implementation ActorAppInterface

+ (void)executeActionWithProto:(NSData*)actionProto
                    completion:(void (^)(NSError* error))completion {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  if (!profile) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolErrorNoProfile
               userInfo:@{NSLocalizedDescriptionKey : @"No profile"}]);
    return;
  }

  actor::ActorService* service =
      actor::ActorServiceFactory::GetForProfile(profile);
  if (!service) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolErrorNoService
               userInfo:@{NSLocalizedDescriptionKey : @"No service"}]);
    return;
  }

  optimization_guide::proto::Action action;
  if (!action.ParseFromArray([actionProto bytes], [actionProto length])) {
    completion([NSError
        errorWithDomain:kActorAppInterfaceErrorDomain
                   code:ActorToolErrorInvalidProto
               userInfo:@{NSLocalizedDescriptionKey : @"Invalid proto"}]);
    return;
  }

  service->ExecuteAction(
      action, base::BindOnce(^(actor::ToolExecutionResult result) {
        if (result.has_value()) {
          completion(nil);
        } else {
          NSString* errorMsg =
              base::SysUTF8ToNSString(GetActorToolErrorMessage(result.error()));
          completion([NSError
              errorWithDomain:@"ActorToolErrorCode"
                         code:(NSInteger)result.error().code
                     userInfo:@{NSLocalizedDescriptionKey : errorMsg}]);
        }
      }));
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

@end
