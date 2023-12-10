// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "ios/web/public/js_messaging/script_message.h"
#import "base/memory/ptr_util.h"
#import "base/values.h"

namespace web {

ScriptMessage::ScriptMessage(std::unique_ptr<base::Value> body,
                             bool is_user_interacting,
                             bool is_main_frame,
                             std::optional<GURL> request_url)
    : body_(std::move(body)),
      is_user_interacting_(is_user_interacting),
      is_main_frame_(is_main_frame),
      request_url_(request_url) {}
ScriptMessage::~ScriptMessage() = default;

ScriptMessage::ScriptMessage(const ScriptMessage& other)
    : is_user_interacting_(other.is_user_interacting_),
      is_main_frame_(other.is_main_frame_),
      request_url_(other.request_url_) {
  if (other.body_) {
    body_ = std::make_unique<base::Value>(other.body_->Clone());
  }
}

}  // namespace web
