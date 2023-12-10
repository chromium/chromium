// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/fuzzer_support/fuzzer_util.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "ios/web/public/js_messaging/fuzzer_support/js_message.pb.h"
#include "ios/web/public/js_messaging/script_message.h"

namespace web {
namespace fuzzer {

std::unique_ptr<web::ScriptMessage> ProtoToScriptMessage(
    const web::ScriptMessageProto& proto) {
  std::optional<base::Value> body = base::JSONReader::Read(proto.body());
  std::optional<GURL> url;
  if (proto.has_url()) {
    url = GURL(proto.url());
  }
  auto script_message = std::make_unique<web::ScriptMessage>(
      (body ? base::Value::ToUniquePtrValue(std::move(*body))
            : std::make_unique<base::Value>()),
      proto.user_interacting(), proto.main_frame(), url);

  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    LOG(WARNING) << "Body: " << *script_message->body();
    LOG(WARNING) << "is_user_interacting: "
                 << script_message->is_user_interacting();
    LOG(WARNING) << "is_main_frame: " << script_message->is_main_frame();
    std::optional<GURL> request_url = script_message->request_url();
    LOG(WARNING) << "request_url: "
                 << (request_url ? request_url.value() : GURL());
  }
  return script_message;
}

}  // namespace fuzzer
}  // namespace web
