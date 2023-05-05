// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/ios_web_message_host_factory.h"

#import <string>

#import "base/functional/overloaded.h"
#import "components/js_injection/browser/js_communication_host.h"
#import "components/js_injection/browser/web_message.h"
#import "components/js_injection/browser/web_message_host.h"
#import "third_party/abseil-cpp/absl/types/variant.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// Created when a message is received from JavaScript.
class IOSWebMessageHost : public js_injection::WebMessageHost {
 public:
  IOSWebMessageHost(const std::string& origin_string, bool is_main_frame) {}

  ~IOSWebMessageHost() override = default;

  // js_injection::WebMessageHost:
  void OnPostMessage(
      std::unique_ptr<js_injection::WebMessage> web_message) override {
    absl::visit(
        base::Overloaded{
            [](const std::u16string& str) {
              // TODO(crbug.com/1423527): Pass on the extracted message to
              // feature code.
              LOG(ERROR) << "webkitMessageHandler received: " << str;
            },
            [](const std::unique_ptr<blink::WebMessageArrayBufferPayload>&
                   array_buffer) {
              // Do nothing if the received message is not a string.
            }},
        web_message->message);
  }
};

}  // namespace

IOSWebMessageHostFactory::IOSWebMessageHostFactory() = default;

IOSWebMessageHostFactory::~IOSWebMessageHostFactory() = default;

std::unique_ptr<js_injection::WebMessageHost>
IOSWebMessageHostFactory::CreateHost(
    const std::string& origin_string,
    bool is_main_frame,
    js_injection::WebMessageReplyProxy* proxy) {
  return std::make_unique<IOSWebMessageHost>(origin_string, is_main_frame);
}

}  // namespace web
