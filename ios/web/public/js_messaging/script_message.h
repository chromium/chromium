// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_H_

#include <memory>
#include <optional>

#include "base/values.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web {

// Represents a script message sent from JavaScript.
class ScriptMessage {
 public:
  explicit ScriptMessage(std::unique_ptr<base::Value> body,
                         bool is_user_interacting,
                         bool is_main_frame,
                         std::optional<GURL> request_url,
                         url::Origin security_origin);
  ~ScriptMessage();

  ScriptMessage& operator=(const ScriptMessage&) = delete;
  ScriptMessage(const ScriptMessage&);

  // Returns the message body.
  base::Value* body() const { return body_.get(); }

  // Whether or not the user was interacting with the page when this message
  // was sent.
  bool is_user_interacting() const { return is_user_interacting_; }

  // Whether or not this message came from the main frame.
  bool is_main_frame() const { return is_main_frame_; }

  // The url, if available, of the frame which sent this message.
  std::optional<GURL> request_url() const { return request_url_; }

  // The security origin of the frame which sent this message.
  // Use this, not `request_url`, when making security decisions.
  // See //docs/security/origin-vs-url.md.
  const url::Origin& security_origin() const { return security_origin_; }

 private:
  std::unique_ptr<base::Value> body_;
  bool is_user_interacting_;
  bool is_main_frame_;
  std::optional<GURL> request_url_;
  url::Origin security_origin_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_H_
