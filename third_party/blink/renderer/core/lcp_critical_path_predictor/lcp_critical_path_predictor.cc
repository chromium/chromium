// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"

namespace blink {

LCPCriticalPathPredictor::LCPCriticalPathPredictor(LocalFrame& frame)
    : frame_(&frame),
      host_(frame.DomWindow()),
      task_runner_(frame.GetTaskRunner(TaskType::kInternalLoading)) {
  CHECK(base::FeatureList::IsEnabled(features::kLCPCriticalPathPredictor));
}

LCPCriticalPathPredictor::~LCPCriticalPathPredictor() = default;

bool LCPCriticalPathPredictor::HasAnyHintData() const {
  return !lcp_element_locators_.empty();
}

void LCPCriticalPathPredictor::set_lcp_element_locators(
    Vector<ElementLocator> locators) {
  CHECK(lcp_element_locators_.empty());
  lcp_element_locators_ = std::move(locators);
}

void LCPCriticalPathPredictor::OnLargestContentfulPaintUpdated(
    Element* lcp_element) {
  if (lcp_element) {
    GetHost().SetLcpElementLocator(
        element_locator::OfElement(lcp_element)->SerializeAsString());
  }
}

mojom::blink::LCPCriticalPathPredictorHost&
LCPCriticalPathPredictor::GetHost() {
  if (!host_.is_bound()) {
    GetFrame().GetBrowserInterfaceBroker().GetInterface(
        host_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return *host_.get();
}

void LCPCriticalPathPredictor::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(host_);
}

}  // namespace blink
