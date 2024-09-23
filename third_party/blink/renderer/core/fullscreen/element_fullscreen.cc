// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fullscreen/element_fullscreen.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

ScriptPromise<IDLUndefined> ElementFullscreen::requestFullscreen(
    ScriptState* script_state,
    Element& element,
    const FullscreenOptions* options,
    ExceptionState& exception_state) {
  return Fullscreen::RequestFullscreen(element, options,
                                       FullscreenRequestType::kUnprefixed,
                                       script_state, &exception_state);
}

void ElementFullscreen::webkitRequestFullscreen(Element& element) {
  FullscreenOptions* options = FullscreenOptions::Create();
  options->setNavigationUI("hide");
  webkitRequestFullscreen(element, options);
}

void ElementFullscreen::webkitRequestFullscreen(
    Element& element,
    const FullscreenOptions* options) {
  if (element.IsInShadowTree()) {
    UseCounter::Count(element.GetDocument(),
                      WebFeature::kPrefixedElementRequestFullscreenInShadow);
  }
  Fullscreen::RequestFullscreen(element, options,
                                FullscreenRequestType::kPrefixed);
}

}  // namespace blink
