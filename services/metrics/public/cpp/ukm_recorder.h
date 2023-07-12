// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"
#include "url/gurl.h"

class DIPSNavigationHandle;
class DIPSService;
class PermissionUmaUtil;
class WebApkUkmRecorder;

namespace apps {
class WebsiteMetrics;
}  // namespace apps

namespace metrics {
class UkmRecorderInterface;
}  // namespace metrics

namespace content {
class FedCmMetrics;
class PaymentAppProviderUtil;
class RenderFrameHostImpl;
}  // namespace content

namespace extensions {
class ExtensionMessagePort;
}

namespace weblayer {
class BackgroundSyncDelegateImpl;
}

namespace ukm {

class DelegatingUkmRecorder;
class TestRecordingHelper;
class UkmBackgroundRecorderService;

enum class AppType {
  kArc,
  kPWA,
  kExtension,
  kChromeApp,
  kCrostini,
  kBorealis,
};

namespace internal {
class SourceUrlRecorderWebContentsObserver;
}  // namespace internal

// This feature controls whether UkmService should be created.
METRICS_EXPORT BASE_DECLARE_FEATURE(kUkmFeature);

// This feature controls whether MojoUkmRecorder gets to decide whether to send
// an IPC for AddEntry.
METRICS_EXPORT BASE_DECLARE_FEATURE(kUkmReduceAddEntryIPC);

// Interface for recording UKM
class METRICS_EXPORT UkmRecorder {
 public:
  UkmRecorder();

  UkmRecorder(const UkmRecorder&) = delete;
  UkmRecorder& operator=(const UkmRecorder&) = delete;

  virtual ~UkmRecorder();

  // Provides access to a global UkmRecorder instance for recording metrics.
  // This is typically passed to the Record() method of an entry object from
  // ukm_builders.h.
  // Use TestAutoSetUkmRecorder for capturing data written this way in tests.
  static UkmRecorder* Get();

  // Get the new SourceId, which is unique for the duration of a browser
  // session.
  static SourceId GetNewSourceID();

  // Gets new source Id for WEBAPK_ID type and updates the manifest URL. This
  // method should only be called by WebApkUkmRecorder class.
  static SourceId GetSourceIdForWebApkManifestUrl(
      base::PassKey<WebApkUkmRecorder>,
      const GURL& manifest_url);

  // Gets new source Id for PAYMENT_APP_ID type and updates the source URL to
  // the scope of the app. This method should only be called by
  // PaymentAppProviderUtil class when the payment app window is opened.
  static SourceId GetSourceIdForPaymentAppFromScope(
      base::PassKey<content::PaymentAppProviderUtil>,
      const GURL& payment_app_from_scope_url);

  // Gets a new SourceId for WEB_IDENTITY_ID type and updates the source URL
  // from the identity provider. This method should only be called in the
  // FedCmMetrics class.
  static SourceId GetSourceIdForWebIdentityFromScope(
      base::PassKey<content::FedCmMetrics>,
      const GURL& provider_url);

  // Gets a new SourceId of REDIRECT_ID type and updates the source URL
  // from the redirect chain. This method should only be called in the
  // DIPSNavigationHandle class.
  static SourceId GetSourceIdForRedirectUrl(base::PassKey<DIPSNavigationHandle>,
                                            const GURL& redirect_url);

  // Gets a new SourceId of EXTENSION_ID type and updates the source URL
  // from the extension message port. This method should only be called in the
  // ExtensionMessagePort class.
  static SourceId GetSourceIdForExtensionUrl(
      base::PassKey<extensions::ExtensionMessagePort>,
      const GURL& extension_url);

  // Gets a new SourceId of REDIRECT_ID type and updates the source URL to the
  // given domain. This method should only be called in the DIPSService class
  // for sites in the DIPS database. `site` must be a registrable domain.
  static SourceId GetSourceIdForDipsSite(base::PassKey<DIPSService>,
                                         const std::string& site);

  // Gets a new SourceId of CHROMEOS_WEBSITE_ID type. This should be only
  // used for recording ChromeOS website stats.
  static SourceId GetSourceIdForChromeOSWebsiteURL(
      base::PassKey<apps::WebsiteMetrics>,
      const GURL& chromeos_website_url);

  // Add an entry to the UkmEntry list.
  virtual void AddEntry(mojom::UkmEntryPtr entry) = 0;

  // Controls sampling for testing purposes. Sampling is 1-in-N (N==rate).
  virtual void SetSamplingForTesting(int rate) {}

 protected:
  // Type-safe wrappers for Update<X> functions.
  void RecordOtherURL(ukm::SourceIdObj source_id, const GURL& url);
  void RecordAppURL(ukm::SourceIdObj source_id,
                    const GURL& url,
                    const AppType app_type);

  // Returns a new SourceId for the given GURL and SourceIDType.
  static SourceId GetSourceIdFromScopeImpl(const GURL& scope_url,
                                           SourceIdType type);

 private:
  friend weblayer::BackgroundSyncDelegateImpl;
  friend DelegatingUkmRecorder;
  friend TestRecordingHelper;
  friend UkmBackgroundRecorderService;
  friend metrics::UkmRecorderInterface;
  friend PermissionUmaUtil;
  friend content::RenderFrameHostImpl;

  // Associates the SourceId with a URL. Most UKM recording code should prefer
  // to use a shared SourceId that is already associated with a URL, rather
  // than using this API directly. New uses of this API must be audited to
  // maintain privacy constraints. See go/ukm-api.
  virtual void UpdateSourceURL(SourceId source_id, const GURL& url) = 0;

  // Associates the SourceId with an app URL for APP_ID sources. This method
  // should only be called by AppSourceUrlRecorder and DelegatingUkmRecorder.
  virtual void UpdateAppURL(SourceId source_id,
                            const GURL& url,
                            const AppType app_type) = 0;

  // Associates navigation data with the UkmSource keyed by |source_id|. This
  // should only be called by SourceUrlRecorderWebContentsObserver, for
  // navigation sources.
  virtual void RecordNavigation(
      SourceId source_id,
      const UkmSource::NavigationData& navigation_data) = 0;

  // Marks a source as no longer needed to keep alive in memory. Called by
  // SourceUrlRecorderWebContentsObserver and AppSourceUrlRecorder (and possibly
  // others in the future) when a browser tab, its WebContents, or a ChromeOS
  // app is no longer alive, implying that no more metrics will be recorded for
  // this source. This reduces UkmRecorder's memory usage. Not to be used
  // through mojo interface.
  virtual void MarkSourceForDeletion(ukm::SourceId source_id) = 0;
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_H_
