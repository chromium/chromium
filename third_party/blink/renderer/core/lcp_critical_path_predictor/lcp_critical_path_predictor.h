// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class LocalFrame;
class Element;

// The LCPCriticalPathPredictor optimizes page load experience by utilizing
// data collected by previous page loads. It sources hint data to various parts
// of Blink to optimize perceived page load speed, and sends the signals
// collected from the current page load to be persisted to the database.
class CORE_EXPORT LCPCriticalPathPredictor final
    : public GarbageCollected<LCPCriticalPathPredictor> {
 public:
  explicit LCPCriticalPathPredictor(LocalFrame& frame);
  virtual ~LCPCriticalPathPredictor();

  LCPCriticalPathPredictor(const LCPCriticalPathPredictor&) = delete;
  LCPCriticalPathPredictor& operator=(const LCPCriticalPathPredictor&) = delete;

  void OnLargestContentfulPaintUpdated(Element* lcp_element);

  void Trace(Visitor*) const;

 private:
  LocalFrame& GetFrame() { return *frame_.Get(); }

  Member<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_
