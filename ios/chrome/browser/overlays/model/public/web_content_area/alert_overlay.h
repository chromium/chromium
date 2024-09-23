// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_ALERT_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_ALERT_OVERLAY_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/model/public/overlay_response_info.h"

class OverlayResponse;
@class TextFieldConfiguration;

namespace alert_overlays {

// Overlays that use alert views should construct an AlertRequest as auxiliary
// data for their feature-specific OverlayRequestConfig.  This allows a single
// UI implementation to be shared across all alert overlays while encapsulating
// this UI implementation knowledge from model code that creates OverlayRequests
// or observes their presentation.
//
// Usage example:
//
// --- in foo_overlay.h ---
// class FooRequest : public OverlayRequestConfig<FooRequest> {
//  public:
//   // ... expose Foo overlay configuration data ...
//  private:
//   void CreateAuxiliaryData(base::SupportsUserData* user_data) override;
// };
//
// class FooResponse : public OverlayResponseInfo<FooResponse> {
//  public:
//   // ... expose Foo overlay user interaction data ...
// };
//
// --- in foo_overlay.mm ---
// std::unique_ptr<OverlayResponse> CreateFooResponse(
//     std::unique_ptr<OverlayResponse> response) {
//   AlertResponse* alert_response = response->GetInfo<AlertResponse>();
//   return OverlayResponse::CreateWithInfo<FooResponse>(
//       /* ... parse `alert_response` to create a FooResponse ... */);
// }
//
// void FooRequest::CreateAuxiliaryData(base::SupportsUserData* user_data) {
//   AlertRequest::CreateForUserData(
//       user_data, /* ... use Foo data to populate alert ... */,
//       base::BindRepeating(&CreateFooResponse));
// }

// Callback that converts an OverlayResponse created with an AlertResponse
// to one exposing its feature-specific logic.  For example, a feature asking
// for the email of a user with an alert can convert `alert_response` to an
// OverlayResponse created with an OverlayResponseInfo that exposes the email
// typed into a text field in the alert, rather than requiring the overlay
// requester to parse the AlertResponse's text field values.
typedef base::RepeatingCallback<std::unique_ptr<OverlayResponse>(
    std::unique_ptr<OverlayResponse> alert_response)>
    ResponseConverter;

// Config struct used to set up buttons shown in overlay UI for an AlertRequest.
struct ButtonConfig {
  // Creates a ButtonConfig with `title` and `style`.
  explicit ButtonConfig(NSString* title,
                        UIAlertActionStyle style = UIAlertActionStyleDefault);
  // Creates a ButtonConfig with `title` and `style`. UMA User Action with
  // `user_action_name` will be recorded when this button is tapped.
  ButtonConfig(NSString* title,
               std::string_view user_action_name,
               UIAlertActionStyle style = UIAlertActionStyleDefault);
  ButtonConfig(const ButtonConfig&);
  ButtonConfig() = delete;
  ~ButtonConfig();
  // The button's title.
  NSString* title;
  // Name of UMA User Action to log when the button is tapped.
  std::string_view user_action_name;
  // The button's style.
  UIAlertActionStyle style;
};

// Configuration object for OverlayRequests implemented using alert views.  This
// config should only be instantiated as auxiliary data to feature-specific
// configs that are implemented using alert views.  It should not be used with
// OverlayRequest::CreateWithConfig(), as it exposes UI implementation details
// to model code that requests or observes overlay presentation.
class AlertRequest : public OverlayRequestConfig<AlertRequest> {
 public:
  ~AlertRequest() override;

  // The alert's title.  All AlertRequests must have a title, message, or both.
  NSString* title() const { return title_; }
  // The alert's message.  All AlertRequests must have a title, message, or
  // both.
  NSString* message() const { return message_; }
  // The accessibility identifier to use for the alert view.  Can be nil.
  NSString* accessibility_identifier() const {
    return accessibility_identifier_;
  }
  // The alert's text field configurations.  Can be nil for alerts without text
  // fields.
  NSArray<TextFieldConfiguration*>* text_field_configs() const {
    return text_field_configs_;
  }
  // The button styles, titles and placement, with each child vector being a
  // horizontal list of buttons.  All alerts must have at least one button.
  const std::vector<std::vector<ButtonConfig>>& button_configs() const {
    return button_configs_;
  }
  // Callback that converts an alert-specific OverlayResponse to one exposing
  // its feature-specific logic.
  const ResponseConverter& response_converter() const {
    return response_converter_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(AlertRequest);

  // Constructor called by CreateForUserData(). All arguments are copied to the
  // ivars below.  `title`, `message`, or both must be non-empty strings.
  // `button_configs` must contain at least one ButtonConfig.
  // `response_converter` must be non-null.
  AlertRequest(NSString* title,
               NSString* message,
               NSString* accessibility_identifier,
               NSArray<TextFieldConfiguration*>* text_field_configs,
               const std::vector<std::vector<ButtonConfig>>& button_configs,
               ResponseConverter response_converter);

  NSString* title_ = nil;
  NSString* message_ = nil;
  NSString* accessibility_identifier_ = nil;
  NSArray<TextFieldConfiguration*>* text_field_configs_ = nil;
  const std::vector<std::vector<ButtonConfig>> button_configs_;
  ResponseConverter response_converter_;
};

// Response info used to create completion OverlayResponses for alert overlays.
class AlertResponse : public OverlayResponseInfo<AlertResponse> {
 public:
  ~AlertResponse() override;

  // The row index of the button tapped by the user to close the dialog.
  size_t tapped_button_row_index() const { return tapped_button_row_index_; }
  // The column index of the button tapped by the user to close the dialog.
  size_t tapped_button_column_index() const {
    return tapped_button_column_index_;
  }
  // The values of the text fields when the button was tapped.
  NSArray<NSString*>* text_field_values() const { return text_field_values_; }

 private:
  OVERLAY_USER_DATA_SETUP(AlertResponse);
  AlertResponse(size_t tapped_button_row_index,
                size_t tapped_button_column_index,
                NSArray<NSString*>* text_field_values);

  const size_t tapped_button_row_index_ = 0;
  const size_t tapped_button_column_index_ = 0;
  NSArray<NSString*>* text_field_values_ = nil;
};

}  // alert_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_WEB_CONTENT_AREA_ALERT_OVERLAY_H_
