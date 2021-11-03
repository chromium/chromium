// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/fuzzer_support/fuzzer_util.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "ios/web/public/js_messaging/fuzzer_support/js_message.pb.h"
#include "ios/web/public/js_messaging/script_message.h"

namespace web {
namespace fuzzer {

std::unique_ptr<web::ScriptMessage> ProtoToScriptMessage(
    const web::ScriptMessageProto& proto) {
  absl::optional<base::Value> body = base::JSONReader::Read(proto.body());
  absl::optional<GURL> url;
  if (proto.has_url()) {
    url = GURL(proto.url());
  }
  return std::make_unique<web::ScriptMessage>(
      (body ? base::Value::ToUniquePtrValue(std::move(*body))
            : std::make_unique<base::Value>()),
      proto.user_interacting(), proto.main_frame(), url);
}

}  // namespace fuzzer
}  // namespace web
