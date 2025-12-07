// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SAFE_BROWSING_DELEGATE_H_
#define EXTENSIONS_BROWSER_SAFE_BROWSING_DELEGATE_H_

#include <string>

#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

// Provides access to telemetry and safe browsing services for extensions. The
// base class has default implementations for all methods and can act as a stub.
class SafeBrowsingDelegate {
 public:
  SafeBrowsingDelegate();
  SafeBrowsingDelegate(const SafeBrowsingDelegate&) = delete;
  SafeBrowsingDelegate& operator=(const SafeBrowsingDelegate&) = delete;
  virtual ~SafeBrowsingDelegate();

  // Returns true if chrome extension telemetry service is enabled.
  virtual bool IsExtensionTelemetryServiceEnabled(
      content::BrowserContext* context) const;

  // TODO(anunoy): This is a temporary implementation of notifying the
  // extension telemetry service of the tabs.executeScript API invocation
  // while its usefulness is evaluated.
  virtual void NotifyExtensionApiTabExecuteScript(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const std::string& code) const {}

  // Notifies the extension telemetry service when declarativeNetRequest API
  // rules are added.
  virtual void NotifyExtensionApiDeclarativeNetRequest(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const std::vector<api::declarative_net_request::Rule>& rules) const {}

  // Notifies the extension telemetry service when declarativeNetRequest
  // redirect action is invoked.
  virtual void NotifyExtensionDeclarativeNetRequestRedirectAction(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const GURL& request_url,
      const GURL& redirect_url) const {}

  // Creates password reuse detection manager when new extension web contents
  // are created.
  virtual void CreatePasswordReuseDetectionManager(
      content::WebContents* web_contents) const {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SAFE_BROWSING_DELEGATE_H_
