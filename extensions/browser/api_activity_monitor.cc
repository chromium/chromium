// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api_activity_monitor.h"

#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace extensions {
namespace activity_monitor {

namespace {

Monitor g_event_monitor = nullptr;
Monitor g_function_monitor = nullptr;
WebRequestMonitor g_web_request_monitor = nullptr;

}  // namespace

Monitor GetApiEventMonitor() {
  return g_event_monitor;
}

Monitor GetApiFunctionMonitor() {
  return g_function_monitor;
}

WebRequestMonitor GetWebRequestMonitor() {
  return g_web_request_monitor;
}

void SetApiEventMonitor(Monitor event_monitor) {
  g_event_monitor = event_monitor;
}

void SetApiFunctionMonitor(Monitor function_monitor) {
  g_function_monitor = function_monitor;
}

void SetWebRequestMonitor(WebRequestMonitor web_request_monitor) {
  g_web_request_monitor = web_request_monitor;
}

void OnApiEventDispatched(content::BrowserContext* browser_context,
                          const ExtensionId& extension_id,
                          const std::string& event_name,
                          const base::Value::List& event_args) {
  if (g_event_monitor) {
    g_event_monitor(browser_context, extension_id, event_name, event_args);
  }
}

// Called when an extension calls an API function.
void OnApiFunctionCalled(content::BrowserContext* browser_context,
                         const ExtensionId& extension_id,
                         const std::string& api_name,
                         const base::Value::List& args) {
  if (g_function_monitor) {
    g_function_monitor(browser_context, extension_id, api_name, args);
  }
}

void OnWebRequestApiUsed(content::BrowserContext* browser_context,
                         const ExtensionId& extension_id,
                         const GURL& url,
                         bool is_incognito,
                         const std::string& api_call,
                         base::Value::Dict details) {
  if (g_web_request_monitor) {
    g_web_request_monitor(browser_context, extension_id, url, is_incognito,
                          api_call, std::move(details));
  }
}

}  // namespace activity_monitor
}  // namespace extensions
