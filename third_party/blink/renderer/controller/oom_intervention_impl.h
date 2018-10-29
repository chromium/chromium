// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_

#include "base/files/scoped_file.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/public/platform/oom_intervention.mojom-blink.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class OomInterventionImplTest;

// Implementation of OOM intervention. This pauses all pages by using
// ScopedPagePauser when near-OOM situation is detected.
class CONTROLLER_EXPORT OomInterventionImpl
    : public mojom::blink::OomIntervention {
 public:
  static void Create(mojom::blink::OomInterventionRequest);

  OomInterventionImpl();
  ~OomInterventionImpl() override;

  // mojom::blink::OomIntervention:
  void StartDetection(mojom::blink::OomInterventionHostPtr,
                      mojom::blink::DetectionArgsPtr detection_args,
                      bool renderer_pause_enabled,
                      bool navigate_ads_enabled) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, DetectedAndDeclined);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, CalculateProcessFootprint);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, StopWatchingAfterDetection);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest,
                           ContinueWatchingWithoutDetection);
  FRIEND_TEST_ALL_PREFIXES(OomInterventionImplTest, V1DetectionAdsNavigation);

  // Overridden by test.
  virtual OomInterventionMetrics GetCurrentMemoryMetrics();
  void Check(TimerBase*);

  void ReportMemoryStats(OomInterventionMetrics& current_memory);

  mojom::blink::DetectionArgsPtr detection_args_;

  mojom::blink::OomInterventionHostPtr host_;
  bool renderer_pause_enabled_ = false;
  bool navigate_ads_enabled_ = false;
  TaskRunnerTimer<OomInterventionImpl> timer_;
  std::unique_ptr<ScopedPagePauser> pauser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_OOM_INTERVENTION_IMPL_H_
