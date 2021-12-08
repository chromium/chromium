// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/observer_list_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/sent_report.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class AttributionPolicy;
class StorableTrigger;
class StorableSource;
class WebContents;

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

    virtual void OnSourceDeactivated(
        const AttributionStorage::DeactivatedSource& source) {}

    virtual void OnReportSent(const SentReport& info) {}

    virtual void OnReportDropped(
        const AttributionStorage::CreateReportResult& result) {}
  };

  virtual ~AttributionManager() = default;

  virtual void AddObserver(Observer* observer) = 0;

  virtual void RemoveObserver(Observer* observer) = 0;

  // Persists the given |source| to storage. Called when a navigation
  // originating from a source tag finishes.
  virtual void HandleSource(StorableSource source) = 0;

  // Process a newly registered trigger. Will create and log any new
  // reports to storage.
  virtual void HandleTrigger(StorableTrigger trigger) = 0;

  // Get all sources that are currently stored in this partition. Used for
  // populating WebUI.
  virtual void GetActiveSourcesForWebUI(
      base::OnceCallback<void(std::vector<StorableSource>)> callback) = 0;

  // Get all pending reports that are currently stored in this partition. Used
  // for populating WebUI.
  virtual void GetPendingReportsForWebUI(
      base::OnceCallback<void(std::vector<AttributionReport>)> callback) = 0;

  // Sends all pending reports immediately, and runs |done| once they have all
  // been sent.
  virtual void SendReportsForWebUI(base::OnceClosure done) = 0;

  // Returns the AttributionPolicy that is used to control API policies such
  // as noise.
  virtual const AttributionPolicy& GetAttributionPolicy() const
      WARN_UNUSED_RESULT = 0;

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
