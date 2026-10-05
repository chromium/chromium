// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_permission_request.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_client.h"
#import "ios/web/util/callback_util.h"
#import "ios/web/web_state/web_state_impl.h"

namespace {

// Converts WKMediaCaptureType to an array of Permissions.
NSArray<NSNumber*>* GetPermissionsFromWKMediaCaptureType(
    WKMediaCaptureType media_capture_type) {
  switch (media_capture_type) {
    case WKMediaCaptureTypeCamera:
      return @[ @(web::PermissionCamera) ];
    case WKMediaCaptureTypeMicrophone:
      return @[ @(web::PermissionMicrophone) ];
    case WKMediaCaptureTypeCameraAndMicrophone:
      return @[ @(web::PermissionCamera), @(web::PermissionMicrophone) ];
  }
}

// Returns the presenting WebState from `presenter` or nullptr if the presenter
// is nil or the WebState has been destroyed.
web::WebStateImpl* GetPresentingWebState(id<CRWPermissionPresenter> presenter) {
  if (!presenter) {
    return nullptr;
  }

  web::WebStateImpl* web_state_impl = presenter.presentingWebState;
  if (!web_state_impl || web_state_impl->IsBeingDestroyed()) {
    return nullptr;
  }

  return web_state_impl;
}

// Returns `decision` if `presenter` is not nil or WKPermissionDecisionDeny
// otherwise.
WKPermissionDecision GetFinalDecision(id<CRWPermissionPresenter> presenter,
                                      WKPermissionDecision decision) {
  return presenter ? decision : WKPermissionDecisionDeny;
}

// Displays the prompt for media capture using `presenter` for `origin`,
// and `media_capture_type` and invokes `callback` with the result.
void DisplayPromptForMediaCapture(
    id<CRWPermissionPresenter> presenter,
    WKMediaCaptureType media_capture_type,
    const GURL& origin,
    base::OnceCallback<void(WKPermissionDecision)> callback) {
  if (web::WebStateImpl* web_state_impl = GetPresentingWebState(presenter)) {
    web::GetWebClient()->WillDisplayMediaCapturePermissionPrompt(
        web_state_impl);
  }

  // Calling WillDisplayMediaCapturePermissionPrompt() may destroy the
  // WebStateImpl, so re-access the pointer from the presenter.
  if (web::WebStateImpl* web_state_impl = GetPresentingWebState(presenter)) {
    __weak id<CRWPermissionPresenter> weak_presenter = presenter;
    web_state_impl->RequestPermissionsWithDecisionHandler(
        GetPermissionsFromWKMediaCaptureType(media_capture_type), origin,
        base::CallbackToBlock(base::BindOnce(&GetFinalDecision, weak_presenter)
                                  .Then(std::move(callback))));
    return;
  }

  // If this point is reached, then the prompt could not be displayed, so
  // invoke the callback with WKPermissionDecisionDeny.
  std::move(callback).Run(WKPermissionDecisionDeny);
}

// Displays the prompt for gelocation using `presenter` for `origin` and
// invokes `callback` with the result.
void DisplayPromptForGeolocation(
    id<CRWPermissionPresenter> presenter,
    const GURL& origin,
    base::OnceCallback<void(WKPermissionDecision)> callback) {
  if (web::WebStateImpl* web_state_impl = GetPresentingWebState(presenter)) {
    __weak id<CRWPermissionPresenter> weak_presenter = presenter;
    web_state_impl->RequestGeolocationPermissionWithDecisionHandler(
        origin,
        base::CallbackToBlock(base::BindOnce(&GetFinalDecision, weak_presenter)
                                  .Then(std::move(callback))));
    return;
  }

  // If this point is reached, then the prompt could not be displayed, so
  // invoke the callback with WKPermissionDecisionDeny.
  std::move(callback).Run(WKPermissionDecisionDeny);
}

}  // namespace

@implementation CRWPermissionRequest {
  // The object that initiates the presentation of the permission prompt.
  __weak id<CRWPermissionPresenter> _presenter;
  // Task runner the decision handler should run on.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  // Handler of user's permission decision.
  base::OnceCallback<void(WKPermissionDecision)> _decisionCallback;
}

- (instancetype)
    initWithPresenter:(id<CRWPermissionPresenter>)presenter
      decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler
         onTaskRunner:
             (const scoped_refptr<base::SequencedTaskRunner>&)taskRunner {
  CHECK(taskRunner, base::NotFatalUntil::M160);
  CHECK(decisionHandler, base::NotFatalUntil::M160);
  if ((self = [super init])) {
    _presenter = presenter;
    _taskRunner = taskRunner;

    // WebKit asserts that `decisionHandler` is called or terminates the app.
    // Use EnsureBlockCalled(...) to call it with WKPermissionDecisionDeny
    // if anything prevents the block from being called directly (e.g. if
    // the TaskRunner rejects the PostTask because the application is in its
    // shutdown phase).
    _decisionCallback =
        web::EnsureBlockCalled(decisionHandler, WKPermissionDecisionDeny);
  }
  return self;
}

- (void)dealloc {
  // Deny permission if decision handler has never been invoked.
  if (_decisionCallback) {
    _taskRunner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(_decisionCallback), WKPermissionDecisionDeny));
  }
}

- (void)displayPromptForMediaCaptureType:(WKMediaCaptureType)mediaCaptureType
                                  origin:(const GURL&)origin {
  if (!_decisionCallback) {
    return;
  }

  // Wrap `_decisionCallback` in base::BindPostTask(...) to ensure WebKit is
  // not informed of the decision synchronously, and prevent stack overflow
  // if invoking the callback results in another permission prompt.
  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DisplayPromptForMediaCapture, _presenter, mediaCaptureType, origin,
          base::BindPostTask(_taskRunner, std::move(_decisionCallback))));
}

- (void)displayPromptForGeolocationOrigin:(const GURL&)origin {
  if (!_decisionCallback) {
    return;
  }

  // Wrap `_decisionCallback` in base::BindPostTask(...) to ensure WebKit is
  // not informed of the decision synchronously, and prevent stack overflow
  // if invoking the callback results in another permission prompt.
  _taskRunner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DisplayPromptForGeolocation, _presenter, origin,
          base::BindPostTask(_taskRunner, std::move(_decisionCallback))));
}

@end
