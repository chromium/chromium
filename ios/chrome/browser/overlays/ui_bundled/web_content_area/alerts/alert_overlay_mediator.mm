// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/alerts/alert_overlay_mediator.h"

#import <string_view>

#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_consumer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;

@interface AlertOverlayMediator ()
// Returns the alert request config from the OverlayRequest used to initialize
// the mediator.
@property(nonatomic, readonly) AlertRequest* alertRequest;
// Returns the alert actions to provide to the consumer.  Constructed using the
// AlertRequest.
@property(nonatomic, readonly) NSArray<NSArray<AlertAction*>*>* alertActions;
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

  [_consumer setTitle:alertRequest->title()];
  [_consumer setMessage:alertRequest->message()];
  [_consumer setTextFieldConfigurations:alertRequest->text_field_configs()];
  NSArray<NSArray<AlertAction*>*>* alertActions = self.alertActions;
  DCHECK_GT(alertActions.count, 0U);
  [_consumer setActions:alertActions];
  [_consumer
      setAlertAccessibilityIdentifier:alertRequest->accessibility_identifier()];
}

- (AlertRequest*)alertRequest {
  return self.request ? self.request->GetConfig<AlertRequest>() : nullptr;
}

- (NSArray<NSArray<AlertAction*>*>*)alertActions {
  AlertRequest* alertRequest = self.alertRequest;
  if (!alertRequest || !alertRequest->button_configs().size())
    return nil;
  const std::vector<std::vector<alert_overlays::ButtonConfig>>& buttonConfigs =
      alertRequest->button_configs();
  size_t rowCount = buttonConfigs.size();
  NSMutableArray<NSArray<AlertAction*>*>* actions =
      [[NSMutableArray<NSArray<AlertAction*>*> alloc]
          initWithCapacity:rowCount];
  for (size_t i = 0; i < rowCount; ++i) {
    const std::vector<alert_overlays::ButtonConfig> buttonConfigsForRow =
        buttonConfigs[i];
    size_t columnCount = buttonConfigsForRow.size();
    DCHECK_GT(columnCount, 0U);
    NSMutableArray<AlertAction*>* actionsForRow =
        [[NSMutableArray<AlertAction*> alloc] initWithCapacity:columnCount];
    for (size_t j = 0; j < columnCount; ++j) {
      [actionsForRow
          addObject:[AlertAction
                        actionWithTitle:buttonConfigsForRow[j].title
                                  style:buttonConfigsForRow[j].style
                                handler:[self actionForButtonAtIndexRow:i
                                                                 column:j]]];
    }
    [actions addObject:actionsForRow];
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

// Sets a completion OverlayResponse after the button at `tappedButtonIndexRow`
// and `tappedButtonIndexColumn` was tapped.
- (void)setCompletionResponseWithRow:(size_t)tappedButtonIndexRow
                              column:(size_t)tappedButtonIndexColumn {
  AlertRequest* alertRequest = self.alertRequest;
  if (!alertRequest)
    return;
  std::unique_ptr<OverlayResponse> alertResponse =
      OverlayResponse::CreateWithInfo<AlertResponse>(
          tappedButtonIndexRow, tappedButtonIndexColumn, self.textFieldValues);
  self.request->GetCallbackManager()->SetCompletionResponse(
      alertRequest->response_converter().Run(std::move(alertResponse)));
  // The response converter should convert the AlertResponse into a feature-
  // specific OverlayResponseInfo type.
  OverlayResponse* convertedResponse =
      self.request->GetCallbackManager()->GetCompletionResponse();
  DCHECK(!convertedResponse || !convertedResponse->GetInfo<AlertResponse>());
}

// Returns the action block for the button at index `row` and `column`.
- (void (^)(AlertAction* action))actionForButtonAtIndexRow:(size_t)row
                                                    column:(size_t)column {
  __weak __typeof__(self) weakSelf = self;
  std::string_view actionName =
      self.alertRequest->button_configs()[row][column].user_action_name;
  return ^(AlertAction*) {
    if (!actionName.empty()) {
      base::RecordComputedAction(actionName.data());
    }
    __typeof__(self) strongSelf = weakSelf;
    [strongSelf setCompletionResponseWithRow:row column:column];
    [strongSelf.delegate stopOverlayForMediator:strongSelf];
  };
}

@end
