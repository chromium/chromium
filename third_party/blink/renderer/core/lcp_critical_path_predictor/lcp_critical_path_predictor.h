// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom-blink.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.pb.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_script_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

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

  // Member functions invoked in LCPP hint consumption path (read path):

  // Checks if `this` has some hint data available for the page load.
  // Meant to be used as preconditions on metrics.
  bool HasAnyHintData() const;

  void set_lcp_element_locators(Vector<ElementLocator> locators);

  const Vector<ElementLocator>& lcp_element_locators() {
    return lcp_element_locators_;
  }

  // Member functions invoked in LCPP hint production path (write path):

  void OnLargestContentfulPaintUpdated(Element* lcp_element);
  LCPScriptObserver* lcp_script_observer() { return lcp_script_observer_; }
  void Trace(Visitor*) const;

 private:
  LocalFrame& GetFrame() { return *frame_.Get(); }
  mojom::blink::LCPCriticalPathPredictorHost& GetHost();

  Member<LocalFrame> frame_;
  HeapMojoRemote<mojom::blink::LCPCriticalPathPredictorHost> host_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Member<LCPScriptObserver> lcp_script_observer_;

  // LCPP hints for consumption (read path):

  Vector<ElementLocator> lcp_element_locators_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_
