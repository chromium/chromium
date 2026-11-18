// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_request_handler.h"

#import <string>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/memory/weak_ptr.h"
#import "base/task/thread_pool.h"
#import "base/types/expected.h"
#import "ios/chrome/app/actor_request.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_callback_util.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_payload_parser.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_precondition_checker.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"

namespace {

using DecryptionResult =
    base::expected<remote_actor::proto::RemoteActorPasswordUploadRequest,
                   RemoteActorParsingError>;

// Decrypts `payload` and `nonce` on a background sequence.
// This is needed because `ParseRemoteActorPayload()` has two overloads.
DecryptionResult DecryptRemoteActorPayload(std::string payload,
                                           std::string nonce) {
  return ParseRemoteActorPayload(payload, nonce);
}

// Validates the preconditions and presents the Remote Actor credential bottom
// sheet in `browser` for the decrypted `request`.
void ProcessDecryptedRemoteActorRequest(
    Browser* browser,
    const remote_actor::proto::RemoteActorPasswordUploadRequest& request) {
  CHECK(browser);
  base::expected<void, RemoteActorPreconditionError> precondition_result =
      CheckRemoteActorPreconditions(browser->GetProfile(), request.gaia_id());
  if (!precondition_result.has_value()) {
    RemoteActorPreconditionCallbackParams callback_params =
        GetPreconditionFailureCallbackParams(precondition_result.error());
    OpenGeminiCallbackURL(request.task_id(), callback_params.status,
                          callback_params.reason, base::DoNothing());
    return;
  }

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  if (![dispatcher dispatchingForProtocol:@protocol(SceneCommands)]) {
    OpenGeminiCallbackURL(request.task_id(), RemoteActorCallbackStatus::kFailed,
                          RemoteActorCallbackReason::kUploadFailed,
                          base::DoNothing());
    return;
  }
  [HandlerForProtocol(dispatcher, SceneCommands)
      showRemoteActorCredentialBottomSheetWithRequest:request];
}

// Handles the result of the background decryption of a Remote Actor request.
void HandleDecryptedRemoteActorPayload(base::WeakPtr<Browser> weak_browser,
                                       DecryptionResult result) {
  Browser* browser = weak_browser.get();
  if (!browser || !result.has_value()) {
    return;
  }
  ProcessDecryptedRemoteActorRequest(browser, *result);
}

}  // namespace

void HandleRemoteActorCredentialSharingRequest(Browser* browser,
                                               RemoteActorRequest request) {
  if (!browser) {
    return;
  }
  if (request.request.has_value()) {
    ProcessDecryptedRemoteActorRequest(browser, *request.request);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&DecryptRemoteActorPayload, std::move(request.payload),
                     std::move(request.nonce)),
      base::BindOnce(&HandleDecryptedRemoteActorPayload, browser->AsWeakPtr()));
}
