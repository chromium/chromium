// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/test/web_extension_app_interface.h"

#import "base/check.h"
#import "base/command_line.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"
#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"
#import "ios/chrome/browser/web_extension/model/fake_extension_service.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/test_switches.h"

namespace {

ProfileState* GetOriginalProfileState() {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  for (ProfileState* profileState in
       [chrome_test_util::GetMainController().appState profileStates]) {
    if (profileState.profile == profile) {
      return profileState;
    }
  }
  return nil;
}

FakeExtensionService* GetFakeExtensionService() {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      test_switches::kEnableFakeExtensionService));
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  ExtensionService* service = ExtensionServiceFactory::GetForProfile(profile);
  return static_cast<FakeExtensionService*>(service);
}

}  // namespace

@implementation WebExtensionAppInterface

+ (BOOL)isExtensionServiceReady {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  ExtensionService* service = ExtensionServiceFactory::GetForProfile(profile);
  return service && service->IsReady();
}

+ (BOOL)isExtensionServiceWaiting {
  FakeExtensionService* fakeService = GetFakeExtensionService();
  return fakeService && fakeService->HasWaitingCallback();
}

+ (BOOL)wasExtensionServiceWaitedUpon {
  FakeExtensionService* fakeService = GetFakeExtensionService();
  return fakeService && fakeService->WasWaitedUpon();
}

+ (void)setExtensionServiceReady:(BOOL)ready {
  FakeExtensionService* fakeService = GetFakeExtensionService();
  if (fakeService) {
    fakeService->SetReady(ready);
  }
}

+ (BOOL)isProfileAtPrepareUIStage {
  ProfileState* profileState = GetOriginalProfileState();
  return profileState && profileState.initStage == ProfileInitStage::kPrepareUI;
}

+ (BOOL)isProfileUIReady {
  ProfileState* profileState = GetOriginalProfileState();
  return profileState && profileState.initStage >= ProfileInitStage::kUIReady;
}

+ (BOOL)isProfileAtFinalStage {
  ProfileState* profileState = GetOriginalProfileState();
  return profileState && profileState.initStage == ProfileInitStage::kFinal;
}

@end
