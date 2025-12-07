// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RENDER_BLOCKING_METRICS_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RENDER_BLOCKING_METRICS_REPORTER_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;

class RenderBlockingMetricsReporter final
    : public GarbageCollected<RenderBlockingMetricsReporter> {
 public:
  static RenderBlockingMetricsReporter& From(Document&);

  explicit RenderBlockingMetricsReporter(Document&);
  RenderBlockingMetricsReporter(const RenderBlockingMetricsReporter&) = delete;
  RenderBlockingMetricsReporter& operator=(
      const RenderBlockingMetricsReporter&) = delete;

  void RenderBlockingResourcesLoaded();
  void PreloadedFontStartedLoading();
  void PreloadedFontFinishedLoading();

  void Trace(Visitor*) const;

 private:
  base::TimeDelta GetDeltaFromTimeOrigin();
  void Report();

  Member<Document> document_;

  int pending_preloaded_fonts_ = 0;
  // Ensure that we don't report multiple times, in case some late preloaded
  // fonts start preloading after others have already finished. If that happens,
  // we ignore those late preloaded fonts, as they are clearly not critical.
  bool preloaded_fonts_reported_ = false;

  bool render_blocking_resources_reported_ = false;
  base::TimeDelta render_blocking_resources_loaded_timestamp_;
  base::TimeDelta preloaded_fonts_loaded_timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_RENDER_BLOCKING_METRICS_REPORTER_H_
