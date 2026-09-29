// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <map>
#import <optional>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler+Testing.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"

@implementation TTCActuationHandler {
  SEQUENCE_CHECKER(_sequenceChecker);

  // The Profile associated with the browser session.
  raw_ptr<ProfileIOS> _profile;

  // The WebStateList associated with the active browser.
  raw_ptr<WebStateList> _webStateList;

  // The SessionID of the browser window.
  std::optional<SessionID> _browserID;

  // Testing override for ActorService.
  raw_ptr<actor::ActorService> _actorServiceForTesting;

  // Map from active Actor task IDs to their controlled WebState IDs.
  std::map<actor::ActorTaskId, web::WebStateID> _taskToWebStateIDMap;
}

#pragma mark - Initializers

- (instancetype)initWithProfile:(ProfileIOS*)profile
                   webStateList:(WebStateList*)webStateList
                      browserID:(SessionID)browserID {
  self = [super init];
  if (self) {
    if (!profile || !webStateList || !browserID.is_valid()) {
      return nil;
    }
    _profile = profile;
    _webStateList = webStateList;
    _browserID = browserID;
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  _webStateList = nullptr;
  _browserID.reset();

  actor::ActorService* actorService = [self actorService];
  if (actorService) {
    for (const auto& [taskID, _] : _taskToWebStateIDMap) {
      actorService->StopTask(taskID, actor::ActorTaskStoppedReason::kShutdown);
    }
  }

  _taskToWebStateIDMap.clear();
  _profile = nullptr;
  _actorServiceForTesting = nullptr;
}

#pragma mark - Public Actuation

- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  actor::ActorTaskId taskID = actor::ActorTaskId();
  actor::ActorService* actorService = [self actorService];
  if (!_webStateList || !actorService) {
    return taskID;
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return taskID;
  }

  if (activeWebState->GetBrowserState() &&
      activeWebState->GetBrowserState()->IsOffTheRecord()) {
    return taskID;
  }

  taskID = actorService->CreateTask(base::SysNSStringToUTF8(title),
                                    /*allow_incognito_web_states=*/false);
  actorService->AddControlledWebState(taskID, activeWebState);
  _taskToWebStateIDMap[taskID] = activeWebState->GetUniqueIdentifier();
  return taskID;
}

#pragma mark - Private

- (actor::ActorService*)actorService {
  if (_actorServiceForTesting) {
    return _actorServiceForTesting;
  }
  if (!_profile) {
    return nullptr;
  }
  return actor::ActorServiceFactory::GetForProfile(_profile);
}

@end

@implementation TTCActuationHandler (Testing)

- (void)setActorServiceForTesting:(actor::ActorService*)actorService {
  _actorServiceForTesting = actorService;
}

@end
