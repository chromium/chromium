// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/debugger/coordinator/aim_debugger_mediator.h"

#import <UIKit/UIKit.h>

#import "base/base64.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "ios/chrome/browser/aim/debugger/ui/aim_debugger_consumer.h"

@implementation AimDebuggerMediator {
  raw_ptr<AimEligibilityService> _service;
  raw_ptr<PrefService> _prefService;
  base::CallbackListSubscription _subscription;
}

- (instancetype)initWithService:(AimEligibilityService*)service
                    prefService:(PrefService*)prefService {
  if ((self = [super init])) {
    _service = service;
    _prefService = prefService;
  }
  return self;
}

- (void)setConsumer:(id<AimDebuggerConsumer>)consumer {
  _consumer = consumer;
  if (_service) {
    __weak AimDebuggerMediator* weakSelf = self;
    _subscription =
        _service->RegisterEligibilityChangedCallback(base::BindRepeating(^{
          [weakSelf updateConsumer];
        }));
    [self updateConsumer];
  }
}

- (void)updateConsumer {
  if (!_consumer || !_service) {
    return;
  }

  AimEligibilitySet eligibility;
  eligibility.PutOrRemove(AimEligibilityCheck::kIsEligible,
                          _service->IsAimEligible());
  eligibility.PutOrRemove(
      AimEligibilityCheck::kIsEligibleByPolicy,
      AimEligibilityService::IsAimAllowedByPolicy(_prefService));
  eligibility.PutOrRemove(AimEligibilityCheck::kIsEligibleByDse,
                          _service->IsAimAllowedByDse());
  eligibility.PutOrRemove(AimEligibilityCheck::kIsServerEligibilityEnabled,
                          _service->IsServerEligibilityEnabled());

  const auto& response = _service->GetMostRecentResponse();
  eligibility.PutOrRemove(AimEligibilityCheck::kIsEligibleByServer,
                          response.is_eligible());

  std::string responseString;
  response.SerializeToString(&responseString);
  NSString* base64 =
      base::SysUTF8ToNSString(base::Base64Encode(responseString));
  [_consumer setServerResponse:base64];

  NSString* source = base::SysUTF8ToNSString(
      AimEligibilityService::EligibilityResponseSourceToString(
          _service->GetMostRecentResponseSource()));
  [_consumer setResponseSource:source];

  [_consumer setEligibilityStatus:eligibility];
}

- (void)disconnect {
  _consumer = nil;
  _service = nil;
  _prefService = nil;
  _subscription = {};
}

#pragma mark - AimDebuggerMutator

- (void)didTapRequestServerEligibility {
  if (_service) {
    _service->StartServerEligibilityRequestForDebugging();
    [self updateConsumer];
  }
}

- (void)didTapApplyResponse:(NSString*)base64Response {
  if (_service && base64Response) {
    _service->SetEligibilityResponseForDebugging(
        base::SysNSStringToUTF8(base64Response));
    [self updateConsumer];
  }
}

- (void)didTapCopyResponse:(NSString*)base64Response {
  UIPasteboard.generalPasteboard.string = base64Response;
  [self.snackbarHandler showSnackbarWithMessage:@"Response Copied"
                                     buttonText:nil
                                  messageAction:nil
                               completionAction:nil];
}

- (void)didTapCopyViewLink:(NSString*)base64Response {
  NSString* url =
      [NSString stringWithFormat:@"http://protoshop/"
                                 @"embed?tabs=textproto&type=gws.searchbox."
                                 @"chrome.AimEligibilityResponse&protobytes=%@",
                                 base64Response];
  UIPasteboard.generalPasteboard.string = url;
  [self.snackbarHandler showSnackbarWithMessage:@"Link Copied"
                                     buttonText:nil
                                  messageAction:nil
                               completionAction:nil];
}

- (void)didTapCopyDraftLink {
  NSString* url =
      @"http://protoshop/gws.searchbox.chrome.AimEligibilityResponse";
  UIPasteboard.generalPasteboard.string = url;
  [self.snackbarHandler showSnackbarWithMessage:@"Link Copied"
                                     buttonText:nil
                                  messageAction:nil
                               completionAction:nil];
}

@end
