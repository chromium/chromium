// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/ios_web_message_host_factory.h"

#import <string>

#import "base/functional/overloaded.h"
#import "base/json/json_reader.h"
#import "base/strings/utf_string_conversions.h"
#import "components/js_injection/browser/js_communication_host.h"
#import "components/js_injection/browser/web_message.h"
#import "components/js_injection/browser/web_message_host.h"
#import "third_party/abseil-cpp/absl/types/variant.h"

namespace web {
namespace {

// Created when a message is received from JavaScript.
class IOSWebMessageHost : public js_injection::WebMessageHost {
 public:
  IOSWebMessageHost(
      const std::string& origin_string,
      bool is_main_frame,
      IOSWebMessageHostFactory::WebMessageCallback message_callback)
      : origin_string_(origin_string),
        is_main_frame_(is_main_frame),
        message_callback_(message_callback) {}

  ~IOSWebMessageHost() override = default;

  // js_injection::WebMessageHost:
  void OnPostMessage(
      std::unique_ptr<js_injection::WebMessage> web_message) override {
    std::optional<std::u16string> received_message;
    absl::visit(
        base::Overloaded{
            [&received_message](const std::u16string& str) {
              received_message = str;
            },
            [](const std::unique_ptr<blink::WebMessageArrayBufferPayload>&
                   array_buffer) {
              // Do nothing if the received message is not a string.
            }},
        web_message->message);
    if (!received_message) {
      return;
    }

    // TODO(crbug.com/40260088): Move this parsing to the renderer process.
    std::optional<base::Value> message_value =
        base::JSONReader::Read(base::UTF16ToUTF8(*received_message));
    if (!message_value) {
      return;
    }
    // TODO(crbug.com/40260088): Determine whether the user has interacted with
    // the source page.
    bool is_user_interacting = false;
    ScriptMessage script_message(
        std::make_unique<base::Value>(std::move(*message_value)),
        is_user_interacting, is_main_frame_, GURL(origin_string_));
    message_callback_.Run(script_message);
  }

 private:
  // The origin of the page that sent the message.
  std::string origin_string_;

  // Whether the page that sent the message is a main frame.
  bool is_main_frame_ = false;

  // Called with the message received from JavaScript.
  IOSWebMessageHostFactory::WebMessageCallback message_callback_;
};

}  // namespace

IOSWebMessageHostFactory::IOSWebMessageHostFactory(
    WebMessageCallback message_callback)
    : message_callback_(message_callback) {}

IOSWebMessageHostFactory::~IOSWebMessageHostFactory() = default;

std::unique_ptr<js_injection::WebMessageHost>
IOSWebMessageHostFactory::CreateHost(
    const std::string& top_level_origin_string,
    const std::string& origin_string,
    bool is_main_frame,
    js_injection::WebMessageReplyProxy* proxy) {
  return std::make_unique<IOSWebMessageHost>(origin_string, is_main_frame,
                                             message_callback_);
}

}  // namespace web
