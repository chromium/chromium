// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/alerts/alert_overlay_mediator.h"

#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view/alert_action.h"
#import "ios/chrome/browser/ui/alert_view/alert_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;

@interface AlertOverlayMediator ()
// Returns the alert request config from the OverlayRequest used to initialize
// the mediator.
@property(nonatomic, readonly) AlertRequest* alertRequest;
// Returns the alert actions to provide to the consumer.  Constructed using the
// AlertRequest.
@property(nonatomic, readonly) NSArray<AlertAction*>* alertActions;
// Returns an array containing the current values of all alert text fields.
@property(nonatomic, readonly) NSArray<NSString*>* textFieldValues;
@end

@implementation AlertOverlayMediator

#pragma mark - Accessors

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  if (!self.request)
    return;

  AlertRequest* alertRequest = self.alertRequest;
  DCHECK(alertRequest);
  DCHECK_GT(alertRequest->title().length + alertRequest->message().length, 0U);

  [_consumer setTitle:alertRequest->title()];
  [_consumer setMessage:alertRequest->message()];
  [_consumer setTextFieldConfigurations:alertRequest->text_field_configs()];
  NSArray<AlertAction*>* alertActions = self.alertActions;
  DCHECK_GT(alertActions.count, 0U);
  [_consumer setActions:alertActions];
  [_consumer
      setAlertAccessibilityIdentifier:alertRequest->accessibility_identifier()];
}

- (AlertRequest*)alertRequest {
  return self.request ? self.request->GetConfig<AlertRequest>() : nullptr;
}

- (NSArray<AlertAction*>*)alertActions {
  AlertRequest* alertRequest = self.alertRequest;
  if (!alertRequest || !alertRequest->button_configs().size())
    return nil;
  const std::vector<alert_overlays::ButtonConfig>& buttonConfigs =
      alertRequest->button_configs();
  size_t buttonCount = buttonConfigs.size();
  NSMutableArray<AlertAction*>* actions =
      [[NSMutableArray<AlertAction*> alloc] initWithCapacity:buttonCount];
  for (size_t i = 0; i < buttonCount; ++i) {
    [actions addObject:[AlertAction
                           actionWithTitle:buttonConfigs[i].title
                                     style:buttonConfigs[i].style
                                   handler:[self actionForButtonAtIndex:i]]];
  }
  return actions;
}

- (NSArray<NSString*>*)textFieldValues {
  AlertRequest* alertRequest = self.alertRequest;
  if (!alertRequest || !alertRequest->text_field_configs().count)
    return nil;

  NSUInteger textFieldCount = alertRequest->text_field_configs().count;
  NSMutableArray<NSString*>* textFieldValues =
      [[NSMutableArray<NSString*> alloc] initWithCapacity:textFieldCount];
  for (NSUInteger i = 0; i < textFieldCount; ++i) {
    NSString* textFieldValue = [self.dataSource textFieldInputForMediator:self
                                                           textFieldIndex:i];
    [textFieldValues addObject:textFieldValue ?: @""];
  }
  return textFieldValues;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return AlertRequest::RequestSupport();
}

#pragma mark - Private

// Sets a completion OverlayResponse after the button at `tappedButtonIndex`
// was tapped.
- (void)setCompletionResponse:(size_t)tappedButtonIndex {
  AlertRequest* alertRequest = self.alertRequest;
  if (!alertRequest)
    return;
  std::unique_ptr<OverlayResponse> alertResponse =
      OverlayResponse::CreateWithInfo<AlertResponse>(tappedButtonIndex,
                                                     self.textFieldValues);
  self.request->GetCallbackManager()->SetCompletionResponse(
      alertRequest->response_converter().Run(std::move(alertResponse)));
  // The response converter should convert the AlertResponse into a feature-
  // specific OverlayResponseInfo type.
  OverlayResponse* convertedResponse =
      self.request->GetCallbackManager()->GetCompletionResponse();
  DCHECK(!convertedResponse || !convertedResponse->GetInfo<AlertResponse>());
}

// Returns the action block for the button at `index`.
- (void (^)(AlertAction* action))actionForButtonAtIndex:(size_t)index {
  __weak __typeof__(self) weakSelf = self;
  base::StringPiece actionName =
      self.alertRequest->button_configs()[index].user_action_name;
  return ^(AlertAction*) {
    if (!actionName.empty()) {
      base::RecordComputedAction(actionName.data());
    }
    __typeof__(self) strongSelf = weakSelf;
    [strongSelf setCompletionResponse:index];
    [strongSelf.delegate stopOverlayForMediator:strongSelf];
  };
}

@end
