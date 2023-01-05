// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"

#import "base/check_op.h"
#import "base/strings/string_piece.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace alert_overlays {

#pragma mark - ButtonConfig

ButtonConfig::~ButtonConfig() = default;

ButtonConfig::ButtonConfig(NSString* title, UIAlertActionStyle style)
    : title(title), style(style) {
  DCHECK_GT(title.length, 0U);
}

ButtonConfig::ButtonConfig(NSString* title,
                           base::StringPiece user_action_name,
                           UIAlertActionStyle style)
    : title(title), user_action_name(user_action_name), style(style) {
  DCHECK_GT(title.length, 0U);
  DCHECK_GT(user_action_name.length(), 0U);
}

ButtonConfig::ButtonConfig(const ButtonConfig& copy) = default;

#pragma mark - AlertRequest

OVERLAY_USER_DATA_SETUP_IMPL(AlertRequest);

AlertRequest::AlertRequest(
    NSString* title,
    NSString* message,
    NSString* accessibility_identifier,
    NSArray<TextFieldConfiguration*>* text_field_configs,
    const std::vector<std::vector<ButtonConfig>>& button_configs,
    ResponseConverter response_converter)
    : title_(title),
      message_(message),
      accessibility_identifier_(accessibility_identifier),
      text_field_configs_(text_field_configs),
      button_configs_(button_configs),
      response_converter_(response_converter) {
  DCHECK(title_.length || message_.length);
  DCHECK_GT(button_configs_.size(), 0U);
  DCHECK(!response_converter.is_null());
}

AlertRequest::~AlertRequest() = default;

#pragma mark - AlertResponse

OVERLAY_USER_DATA_SETUP_IMPL(AlertResponse);

AlertResponse::AlertResponse(size_t tapped_button_row_index,
                             size_t tapped_button_column_index,
                             NSArray<NSString*>* text_field_values)
    : tapped_button_row_index_(tapped_button_row_index),
      tapped_button_column_index_(tapped_button_column_index),
      text_field_values_(text_field_values) {}

AlertResponse::~AlertResponse() = default;

}  // alert_overlays
