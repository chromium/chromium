// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_JS_MESSAGING_IOS_WEB_MESSAGE_HOST_FACTORY_H_
#define IOS_WEB_CONTENT_JS_MESSAGING_IOS_WEB_MESSAGE_HOST_FACTORY_H_

#import "base/functional/callback.h"
#import "base/values.h"
#import "components/js_injection/browser/web_message_host_factory.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace web {

// An iOS-specific implementation of WebMessageHostFactory, responsible for
// creating a WebMessageHost when JavaScript sends a message to the browser.
class IOSWebMessageHostFactory : public js_injection::WebMessageHostFactory {
 public:
  using WebMessageCallback =
      base::RepeatingCallback<void(const ScriptMessage&)>;

  // `message_callback` is called with each message received from JavaScript.
  IOSWebMessageHostFactory(WebMessageCallback message_callback);
  IOSWebMessageHostFactory(const IOSWebMessageHostFactory&) = delete;
  IOSWebMessageHostFactory& operator=(const IOSWebMessageHostFactory&) = delete;
  ~IOSWebMessageHostFactory() override;

  // js_injection::WebMessageHostFactory:
  std::unique_ptr<js_injection::WebMessageHost> CreateHost(
      const std::string& top_level_origin_string,
      const std::string& origin_string,
      bool is_main_frame,
      js_injection::WebMessageReplyProxy* proxy) override;

 private:
  // Called with each message received from JavaScript.
  WebMessageCallback message_callback_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_JS_MESSAGING_IOS_WEB_MESSAGE_HOST_FACTORY_H_
