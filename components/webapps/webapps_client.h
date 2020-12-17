// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_WEBAPPS_CLIENT_H_
#define COMPONENTS_WEBAPPS_WEBAPPS_CLIENT_H_

#include "components/security_state/core/security_state.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace infobars {
class ContentInfoBarManager;
}  // namespace infobars

namespace webapps {

enum class InstallTrigger;
enum class WebappInstallSource;

// Interface to be implemented by the embedder (such as Chrome or WebLayer) to
// expose embedder specific logic.
class WebappsClient {
 public:
  WebappsClient();
  WebappsClient(const WebappsClient&) = delete;
  WebappsClient& operator=(const WebappsClient&) = delete;
  virtual ~WebappsClient();

  // Return the webapps client.
  static WebappsClient* Get();

  virtual security_state::SecurityLevel GetSecurityLevelForWebContents(
      content::WebContents* web_contents) = 0;

  virtual infobars::ContentInfoBarManager* GetInfoBarManagerForWebContents(
      content::WebContents* web_contents) = 0;

  virtual WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      InstallTrigger trigger) = 0;

  virtual bool IsInstallationInProgress(content::WebContents* web_contents,
                                        const GURL& manifest_url) = 0;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_WEBAPPS_CLIENT_H_
