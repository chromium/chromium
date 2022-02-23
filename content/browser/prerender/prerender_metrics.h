// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_METRICS_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_METRICS_H_

#include <string>

#include "base/time/time.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Please update GetCancelledInterfaceType() in the corresponding .cc file
// and the enum of PrerenderCancelledUnknownInterface in
// tools/metrics/histograms/enums.xml if you add a new item.
enum class PrerenderCancelledInterface {
  kUnknown = 0,  // For kCancel interfaces added by embedders or tests.
  kGamepadHapticsManager = 1,
  kGamepadMonitor = 2,
  kNotificationService = 3,
  kSyncEncryptionKeysExtension = 4,
  kMaxValue = kSyncEncryptionKeysExtension
};

// Used by PrerenderNavigationThrottle, to track the cross-origin cancellation
// reason, and break it down into more cases.
// Do not modify this enum.
enum class PrerenderCrossOriginRedirectionMismatch {
  kShouldNotBeReported = 0,
  kPortMismatch = 1,
  kHostMismatch = 2,
  kHostPortMismatch = 3,
  kSchemeMismatch = 4,
  kSchemePortMismatch = 5,
  kSchemeHostMismatch = 6,
  kSchemeHostPortMismatch = 7,
  kMaxValue = kSchemeHostPortMismatch
};

// Used by PrerenderNavigationThrottle. This is a breakdown enum for
// PrerenderCrossOriginRedirectionMismatch.kSchemePortMismatch.
// Do not modify this enum.
enum class PrerenderCrossOriginRedirectionProtocolChange {
  kHttpProtocolUpgrade = 0,
  kHttpProtocolDowngrade = 1,
  kMaxValue = kHttpProtocolDowngrade
};

// Used by PrerenderNavigationThrottle. This is a breakdown enum for
// PrerenderCrossOriginRedirectionMismatch.kHostMismatch.
// Do not modify this enum.
enum class PrerenderCrossOriginRedirectionDomain {
  kRedirectToSubDomain = 0,
  kRedirectFromSubDomain = 1,
  kCrossDomain = 2,
  kMaxValue = kCrossDomain
};

void RecordPrerenderCancelledInterface(
    const std::string& interface_name,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

void RecordPrerenderTriggered(ukm::SourceId ukm_id);

void RecordPrerenderActivationTime(
    base::TimeDelta delta,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// Records the status to UMA and UKM. `initiator_ukm_id` represents the page
// that starts prerendering and `prerendered_ukm_id` represents the prerendered
// page. `prerendered_ukm_id` is valid after the page is activated.
void RecordPrerenderHostFinalStatus(
    PrerenderHost::FinalStatus status,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix,
    ukm::SourceId initiator_ukm_id,
    ukm::SourceId prerendered_ukm_id);

void RecordPrerenderRedirectionMismatchType(
    PrerenderCrossOriginRedirectionMismatch case_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// Records whether the redirection was caused by HTTP protocol upgrade.
void RecordPrerenderRedirectionProtocolChange(
    PrerenderCrossOriginRedirectionProtocolChange change_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

// Records whether the prerendering navigation was redirected to a subdomain
// page.
void RecordPrerenderRedirectionDomain(
    PrerenderCrossOriginRedirectionDomain domain_type,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix);

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_METRICS_H_
