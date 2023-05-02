// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_WEBAPPS_CLIENT_H_
#define COMPONENTS_WEBAPPS_BROWSER_WEBAPPS_CLIENT_H_

#include "build/build_config.h"
#include "components/security_state/core/security_state.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace infobars {
class ContentInfoBarManager;
}  // namespace infobars

namespace url {
class Origin;
}  // namespace url

namespace webapps {

class AppBannerManager;
enum class InstallTrigger;
enum class WebappInstallSource;
struct AddToHomescreenParams;

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

  // Returns true if the given Origin should be considered secure enough to
  // host an app. Returning false signals that other checks should be
  // performed, not that the app is insecure.
  virtual bool IsOriginConsideredSecure(const url::Origin& url) = 0;

  virtual security_state::SecurityLevel GetSecurityLevelForWebContents(
      content::WebContents* web_contents) = 0;

  virtual infobars::ContentInfoBarManager* GetInfoBarManagerForWebContents(
      content::WebContents* web_contents) = 0;

  virtual WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      InstallTrigger trigger) = 0;

  virtual AppBannerManager* GetAppBannerManager(
      content::WebContents* web_contents) = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual bool IsInstallationInProgress(content::WebContents* web_contents,
                                        const GURL& manifest_id) = 0;

  virtual bool CanShowAppBanners(content::WebContents* web_contents) = 0;

  virtual void OnWebApkInstallInitiatedFromAppMenu(
      content::WebContents* web_contents) = 0;

  virtual void InstallWebApk(content::WebContents* web_contents,
                             const AddToHomescreenParams& params) = 0;

  virtual void InstallShortcut(content::WebContents* web_contents,
                               const AddToHomescreenParams& params) = 0;
#endif
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_WEBAPPS_CLIENT_H_
