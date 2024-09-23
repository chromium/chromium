// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_media_capture_permission_request.h"

#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_client.h"
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

}  // namespace

@implementation CRWMediaCapturePermissionRequest {
  // Task runner the decision handler should run on.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  // Handler of user's media capture decision.
  void (^_decisionHandler)(WKPermissionDecision);
  // Track whether the decision handler has been called.
  BOOL _decisionHandlerInvoked;
}

- (instancetype)initWithDecisionHandler:
                    (void (^)(WKPermissionDecision decision))decisionHandler
                           onTaskRunner:
                               (const scoped_refptr<base::SequencedTaskRunner>&)
                                   taskRunner {
  if ((self = [super init])) {
    _taskRunner = taskRunner;
    _decisionHandler = decisionHandler;
    _decisionHandlerInvoked = NO;
  }
  return self;
}

- (void)dealloc {
  // Deny permission if decision handler has never been invoked.
  if (!_decisionHandlerInvoked) {
    [self handleDecision:WKPermissionDecisionDeny];
  }
}

- (void)displayPromptForMediaCaptureType:(WKMediaCaptureType)mediaCaptureType
                                  origin:(const GURL&)origin {
  // This block strongly captures `self` intentionally to ensure that
  // `_decisionHandler` is always invoked, even if the scope of `self` has
  // deallocated.
  __block GURL originCopy = origin;
  _taskRunner->PostTask(
      FROM_HERE, base::BindOnce(^{
        [self displayPromptForMediaCaptureTypeOnTaskRunner:mediaCaptureType
                                                    origin:originCopy];
      }));
}

#pragma mark - Private

// Helper method that only executes on `_taskRunner`'s sequence.
- (void)displayPromptForMediaCaptureTypeOnTaskRunner:
            (WKMediaCaptureType)mediaCaptureType
                                              origin:(const GURL&)origin {
  if (!_presenter) {
    [self handleDecision:WKPermissionDecisionDeny];
    return;
  }
  web::WebStateImpl* webState = _presenter.presentingWebState;
  if (webState && !webState->IsBeingDestroyed()) {
    web::GetWebClient()->WillDisplayMediaCapturePermissionPrompt(webState);

    // Calling WillDisplayMediaCapturePermissionPrompt(...) may cause the
    // WebState to be closed. Fetch the value from the presenter again (as it
    // uses a weak pointer internally) to avoid having a dangling pointer.
    webState = _presenter.presentingWebState;
  }
  // By this point, the WebState may have been destroyed. If this is the case,
  // then `webState->IsBeingDestroyed` will be YES.
  if (!webState || webState->IsBeingDestroyed()) {
    [self handleDecision:WKPermissionDecisionDeny];
    return;
  }

  // This block strongly captures `self` intentionally to ensure that
  // `_decisionHandler` is always invoked, even if the scope of `self` has
  // deallocated.
  webState->RequestPermissionsWithDecisionHandler(
      GetPermissionsFromWKMediaCaptureType(mediaCaptureType), origin,
      ^(WKPermissionDecision decision) {
        [self handleDecision:decision];
      });
}

// Handle user response to media capture request.
- (void)handleDecision:(WKPermissionDecision)decision {
  _decisionHandler(_presenter ? decision : WKPermissionDecisionDeny);
  _decisionHandlerInvoked = YES;
}

@end
