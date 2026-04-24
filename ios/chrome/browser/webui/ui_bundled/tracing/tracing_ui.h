// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TRACING_TRACING_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TRACING_TRACING_UI_H_

#import <Foundation/Foundation.h>

#import <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

class TracingMessageHandler : public web::WebUIIOSMessageHandler {
 public:
  TracingMessageHandler();
  ~TracingMessageHandler() override;

  TracingMessageHandler(const TracingMessageHandler&) = delete;
  TracingMessageHandler& operator=(const TracingMessageHandler&) = delete;

  // WebUIIOSMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleStartTracing(const base::ListValue& args);
  void HandleStopTracing(const base::ListValue& args);
  void OnTracingStarted();
  void OnTracingStopped();

  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  base::WeakPtrFactory<TracingMessageHandler> weak_ptr_factory_{this};
};

// The WebUI handler for chrome://tracing.
class TracingUI : public web::WebUIIOSController {
 public:
  explicit TracingUI(web::WebUIIOS* web_ui, const std::string& host);

  TracingUI(const TracingUI&) = delete;
  TracingUI& operator=(const TracingUI&) = delete;

  ~TracingUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_TRACING_TRACING_UI_H_
