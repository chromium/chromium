// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class AttributionTrigger;
class AttributionDataHostManager;
class StorableSource;
class StoredSource;
class WebContents;

struct SendResult;

// Interface that mediates data flow between the network, storage layer, and
// blink.
class AttributionManager {
 public:
  // Provides access to a AttributionManager implementation. This layer of
  // abstraction is to allow tests to mock out the AttributionManager without
  // injecting a manager explicitly.
  class Provider {
   public:
    virtual ~Provider() = default;

    // Gets the AttributionManager that should be used for handling attributions
    // that occur in the given |web_contents|. Returns nullptr if attribution
    // reporting is not enabled in the given |web_contents|, e.g. when the
    // browser context is off the record.
    virtual AttributionManager* GetManager(WebContents* web_contents) const = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnSourcesChanged() {}

    virtual void OnReportsChanged() {}

    virtual void OnSourceHandled(const StorableSource& source,
                                 StorableSource::Result result) {}

    virtual void OnSourceDeactivated(
        const AttributionStorage::DeactivatedSource& source) {}

    virtual void OnReportSent(const AttributionReport& report,
                              const SendResult& info) {}

    virtual void OnTriggerHandled(
        const AttributionStorage::CreateReportResult& result) {}
  };

  virtual ~AttributionManager() = default;

  virtual void AddObserver(Observer* observer) = 0;

  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets manager responsible for tracking pending data hosts targeting `this`.
  virtual AttributionDataHostManager* GetDataHostManager() = 0;

  // Persists the given |source| to storage. Called when a navigation
  // originating from a source tag finishes.
  virtual void HandleSource(StorableSource source) = 0;

  // Process a newly registered trigger. Will create and log any new
  // reports to storage.
  virtual void HandleTrigger(AttributionTrigger trigger) = 0;

  // Get all sources that are currently stored in this partition. Used for
  // populating WebUI.
  virtual void GetActiveSourcesForWebUI(
      base::OnceCallback<void(std::vector<StoredSource>)> callback) = 0;

  // Get all pending reports that are currently stored in this partition. Used
  // for populating WebUI and simulator.
  virtual void GetPendingReportsForInternalUse(
      base::OnceCallback<void(std::vector<AttributionReport>)> callback) = 0;

  // Sends the given reports immediately, and runs |done| once they have all
  // been sent.
  virtual void SendReportsForWebUI(
      const std::vector<AttributionReport::EventLevelData::Id>& ids,
      base::OnceClosure done) = 0;

  // Deletes all data in storage for URLs matching |filter|, between
  // |delete_begin| and |delete_end| time.
  //
  // If |filter| is null, then consider all origins in storage as matching.
  virtual void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter,
      base::OnceClosure done) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_
