// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_METRICS_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_METRICS_H_

#include <string>

#include "base/time/time.h"
#include "content/browser/preloading/prerender/prerender_host.h"
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
  // kNotificationService = 3,   Deprecated.
  kSyncEncryptionKeysExtension = 4,
  kMaxValue = kSyncEncryptionKeysExtension
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

// Records the status to UMA and UKM, and reports the status other than
// kActivated to DevTools. In the attributes, `initiator_ukm_id` represents the
// page that starts prerendering. `prerendered_ukm_id` represents the
// prerendered page and is valid after the page is activated.
void RecordPrerenderHostFinalStatus(PrerenderHost::FinalStatus status,
                                    const PrerenderAttributes& attributes,
                                    ukm::SourceId prerendered_ukm_id);

// Records which navigation parameters are different between activation and
// initial prerender navigation when activation fails.
void RecordPrerenderActivationNavigationParamsMatch(
    PrerenderHost::ActivationNavigationParamsMatch result,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_suffix);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_METRICS_H_
