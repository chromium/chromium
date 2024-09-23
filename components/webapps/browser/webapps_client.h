// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_WEBAPPS_CLIENT_H_
#define COMPONENTS_WEBAPPS_BROWSER_WEBAPPS_CLIENT_H_

#include <memory>

#include "base/auto_reset.h"
#include "build/build_config.h"
#include "components/security_state/core/security_state.h"
#include "components/webapps/common/web_app_id.h"

class GURL;

namespace blink::mojom {
class Manifest;
}

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace infobars {
class ContentInfoBarManager;
}  // namespace infobars

namespace segmentation_platform {
class SegmentationPlatformService;
}  // namespace segmentation_platform

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

  // Returns if any current installations conflict with a new web app with the
  // given start_url and manifest_id. See the implementing class for more
  // details on their behavior. Returning true here signifies that an app is
  // already installed here.
  virtual bool DoesNewWebAppConflictWithExistingInstallation(
      content::BrowserContext* browsing_context,
      const GURL& start_url,
      const ManifestId& manifest_id) const = 0;

  // Returns if the web contents this manager is on is inside of an app context.
  virtual bool IsInAppBrowsingContext(
      content::WebContents* web_contents) const = 0;

  // Tracks whether the current site URL obtained from the web_contents is not
  // locally installed.
  virtual bool IsAppPartiallyInstalledForSiteUrl(
      content::BrowserContext* browsing_context,
      const GURL& site_url) const = 0;

  // Tracks whether the current site URL obtained from the web_contents is fully
  // installed. The only difference from
  // DoesNewWebAppConflictWithExistingInstallation() is that the former
  // considers the scope obtained from a manifest as check for if an app is
  // already installed.
  virtual bool IsAppFullyInstalledForSiteUrl(
      content::BrowserContext* browsing_context,
      const GURL& site_url) const = 0;
  virtual bool IsUrlControlledBySeenManifest(
      content::BrowserContext* browsing_context,
      const GURL& site_url) const = 0;

  // Called when a manifest is seen by the AppBannerManager. This manifest
  // must be non-empty. Note that this may be the 'default' manifest.
  virtual void OnManifestSeen(content::BrowserContext* browsing_context,
                              const blink::mojom::Manifest& manifest) const = 0;

  // The user has ignored the installation dialog and it went away due to
  // another interaction (e.g. the tab was changed, page navigated, etc).
  virtual void SaveInstallationIgnoredForMl(
      content::BrowserContext* browsing_context,
      const GURL& manifest_id) const = 0;
  // The user has taken active action on the dialog to make it go away.
  virtual void SaveInstallationDismissedForMl(
      content::BrowserContext* browsing_context,
      const GURL& manifest_id) const = 0;
  virtual void SaveInstallationAcceptedForMl(
      content::BrowserContext* browsing_context,
      const GURL& manifest_id) const = 0;
  virtual bool IsMlPromotionBlockedByHistoryGuardrail(
      content::BrowserContext* browsing_context,
      const GURL& manifest_id) const = 0;

  virtual segmentation_platform::SegmentationPlatformService*
  GetSegmentationPlatformService(
      content::BrowserContext* browsing_context) const = 0;

  using ScopedSegmentationServiceOverride = base::AutoReset<
      std::unique_ptr<segmentation_platform::SegmentationPlatformService>>;
  ScopedSegmentationServiceOverride OverrideSegmentationServiceForTesting(
      std::unique_ptr<segmentation_platform::SegmentationPlatformService>
          service);

#if BUILDFLAG(IS_ANDROID)
  virtual bool IsInstallationInProgress(content::WebContents* web_contents,
                                        const GURL& manifest_id) = 0;

  virtual bool CanShowAppBanners(const content::WebContents* web_contents) = 0;

  virtual void OnWebApkInstallInitiatedFromAppMenu(
      content::WebContents* web_contents) = 0;

  virtual void InstallWebApk(content::WebContents* web_contents,
                             const AddToHomescreenParams& params) = 0;

  virtual void InstallShortcut(content::WebContents* web_contents,
                               const AddToHomescreenParams& params) = 0;
#endif

  // Returns the id of the app that controls the last committed url of the given
  // `web_contents`.
  // Note: On Android this always returns `std::nullopt`.
  virtual std::optional<webapps::AppId> GetAppIdForWebContents(
      content::WebContents* web_contents) = 0;

 protected:
  segmentation_platform::SegmentationPlatformService*
  segmentation_platform_for_testing() const {
    return segmentation_platform_for_testing_.get();
  }

 private:
  std::unique_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_for_testing_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_WEBAPPS_CLIENT_H_
