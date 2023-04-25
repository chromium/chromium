// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/common/content_export.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace base {
class Time;
}  // namespace base

namespace content {

class AttributionDataHostManager;
class AttributionObserver;
class AttributionTrigger;
class BrowserContext;
class BrowsingDataFilterBuilder;
class StorableSource;
class StoredSource;
class WebContents;

struct GlobalRenderFrameHostId;

#if BUILDFLAG(IS_ANDROID)
struct OsRegistration;
#endif

// Interface that mediates data flow between the network, storage layer, and
// blink.
class CONTENT_EXPORT AttributionManager : public AttributionDataModel {
 public:
  static AttributionManager* FromWebContents(WebContents* web_contents);

  static AttributionManager* FromBrowserContext(BrowserContext*);

  static network::mojom::AttributionSupport GetSupport();

  ~AttributionManager() override = default;

  virtual void AddObserver(AttributionObserver* observer) = 0;

  virtual void RemoveObserver(AttributionObserver* observer) = 0;

  // Gets manager responsible for tracking pending data hosts targeting `this`.
  virtual AttributionDataHostManager* GetDataHostManager() = 0;

  // Persists the given |source| to storage. Called when a navigation
  // originating from a source tag finishes.
  virtual void HandleSource(StorableSource source,
                            GlobalRenderFrameHostId render_frame_id) = 0;

  // Process a newly registered trigger. Will create and log any new
  // reports to storage.
  virtual void HandleTrigger(AttributionTrigger trigger,
                             GlobalRenderFrameHostId render_frame_id) = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual void HandleOsRegistration(
      OsRegistration,
      GlobalRenderFrameHostId render_frame_id) = 0;
#endif

  // Get all sources that are currently stored in this partition. Used for
  // populating WebUI.
  virtual void GetActiveSourcesForWebUI(
      base::OnceCallback<void(std::vector<StoredSource>)> callback) = 0;

  // Get all pending reports that are currently stored in this partition. Used
  // for populating WebUI and simulator.
  virtual void GetPendingReportsForInternalUse(
      int limit,
      base::OnceCallback<void(std::vector<AttributionReport>)> callback) = 0;

  // Sends the given reports immediately, and runs |done| once they have all
  // been sent.
  virtual void SendReportsForWebUI(
      const std::vector<AttributionReport::Id>& ids,
      base::OnceClosure done) = 0;

  // Notifies observers of a failed browser-side source-registration.
  // Called by `AttributionDataHostManagerImpl`.
  virtual void NotifyFailedSourceRegistration(
      const std::string& header_value,
      const attribution_reporting::SuitableOrigin& source_origin,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      attribution_reporting::mojom::SourceType,
      attribution_reporting::mojom::SourceRegistrationError) = 0;

  // Deletes all data in storage for storage keys matching `filter`, between
  // `delete_begin` and `delete_end` time.
  //
  // If `filter` is null, then consider all storage keys in storage as matching.
  //
  // Precondition: `filter` should be built from the `filter_builder`, as well
  // as any check that requires inspecting the `SpecialStoragePolicy` which
  // isn't covered by `BrowsingDataFilterBuilder`.
  //
  // The only reason `filter_builder` needs to be passed here is for
  // communication with the Android system of the raw data in the filter. Caller
  // maintains ownership of `filter_builder`.
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         StoragePartition::StorageKeyMatcherFunction filter,
                         BrowsingDataFilterBuilder* filter_builder,
                         bool delete_rate_limit_data,
                         base::OnceClosure done) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_
