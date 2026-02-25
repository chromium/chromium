// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_app_interface.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_service.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_service_factory.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"

NSString* const kActuationAppInterfaceErrorDomain =
    @"ActuationAppInterfaceErrorDomain";

@implementation ActuationAppInterface

+ (void)executeActionWithProto:(NSData*)actionProto
                    completion:(void (^)(NSError* error))completion {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  if (!profile) {
    completion([NSError
        errorWithDomain:kActuationAppInterfaceErrorDomain
                   code:ActuationErrorNoProfile
               userInfo:@{NSLocalizedDescriptionKey : @"No profile"}]);
    return;
  }

  ActuationService* service = ActuationServiceFactory::GetForProfile(profile);
  if (!service) {
    completion([NSError
        errorWithDomain:kActuationAppInterfaceErrorDomain
                   code:ActuationErrorNoService
               userInfo:@{NSLocalizedDescriptionKey : @"No service"}]);
    return;
  }

  optimization_guide::proto::Action action;
  if (!action.ParseFromArray([actionProto bytes], [actionProto length])) {
    completion([NSError
        errorWithDomain:kActuationAppInterfaceErrorDomain
                   code:ActuationErrorInvalidProto
               userInfo:@{NSLocalizedDescriptionKey : @"Invalid proto"}]);
    return;
  }

  service->ExecuteAction(
      action, base::BindOnce(^(ActuationTool::ActuationResult result) {
        if (result.has_value()) {
          completion(nil);
        } else {
          NSString* errorMsg =
              base::SysUTF8ToNSString(GetActuationErrorMessage(result.error()));
          completion([NSError
              errorWithDomain:@"ActuationErrorCode"
                         code:(NSInteger)result.error().code
                     userInfo:@{NSLocalizedDescriptionKey : errorMsg}]);
        }
      }));
}

@end
