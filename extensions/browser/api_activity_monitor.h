// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ACTIVITY_MONITOR_H_
#define EXTENSIONS_BROWSER_API_ACTIVITY_MONITOR_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {
namespace activity_monitor {

using Monitor = void (*)(content::BrowserContext* browser_context,
                         const ExtensionId& extension_id,
                         const std::string& activity_name,
                         const base::Value::List& event_args);
using WebRequestMonitor = void (*)(content::BrowserContext* browser_context,
                                   const ExtensionId& extension_id,
                                   const GURL& url,
                                   bool is_incognito,
                                   const std::string& api_call,
                                   base::Value::Dict details);

// Get or set the current global monitor for API events and functions. Note that
// these handlers *must* be allowed to be called on any thread!
// Additionally, since this may be called on any thead, |browser_context| is
// unsafe to use unless posted to the UI thread.
Monitor GetApiEventMonitor();
Monitor GetApiFunctionMonitor();
WebRequestMonitor GetWebRequestMonitor();
void SetApiEventMonitor(Monitor event_monitor);
void SetApiFunctionMonitor(Monitor function_monitor);
void SetWebRequestMonitor(WebRequestMonitor web_request_monitor);

// Called when an API event is dispatched to an extension. May be called on any
// thread. |browser_context| is unsafe to use.
void OnApiEventDispatched(content::BrowserContext* browser_context,
                          const ExtensionId& extension_id,
                          const std::string& event_name,
                          const base::Value::List& event_args);

// Called when an extension calls an API function. May be called on any thread.
// |browser_context| is unsafe to use.
void OnApiFunctionCalled(content::BrowserContext* browser_context,
                         const ExtensionId& extension_id,
                         const std::string& api_name,
                         const base::Value::List& args);

// Called when an extension uses the web request API. May be called on any
// thread. |browser_context| is unsafe to use.
void OnWebRequestApiUsed(content::BrowserContext* browser_context,
                         const ExtensionId& extension_id,
                         const GURL& url,
                         bool is_incognito,
                         const std::string& api_call,
                         base::Value::Dict details);

}  // namespace activity_monitor
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ACTIVITY_MONITOR_H_
