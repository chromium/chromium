// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/script_message.h"

#import <memory>

#import "base/memory/ptr_util.h"
#import "base/values.h"

namespace web {

ScriptMessage::ScriptMessage(std::unique_ptr<base::Value> legacy_body,
                             bool is_user_interacting,
                             bool is_main_frame,
                             std::optional<GURL> request_url,
                             url::Origin security_origin)
    : legacy_body_(std::move(legacy_body)),
      is_user_interacting_(is_user_interacting),
      is_main_frame_(is_main_frame),
      request_url_(request_url),
      security_origin_(std::move(security_origin)) {}
ScriptMessage::~ScriptMessage() = default;

ScriptMessage::ScriptMessage(ScriptMessage&& message)
    : legacy_body_(std::move(message.legacy_body_)),
      is_user_interacting_((message.is_user_interacting_)),
      is_main_frame_(message.is_main_frame_),
      request_url_(std::move(message.request_url_)),
      security_origin_(std::move(message.security_origin_)) {}

}  // namespace web
