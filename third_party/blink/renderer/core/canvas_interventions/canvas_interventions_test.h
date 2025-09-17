// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_TEST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT CanvasInterventionsTest final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CanvasInterventionsTest() = default;
  CanvasInterventionsTest(const CanvasInterventionsTest&) = delete;
  CanvasInterventionsTest& operator=(const CanvasInterventionsTest&) = delete;

  // noise_interventions_test.idl
  static String getCanvasNoiseToken(ExecutionContext*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CANVAS_INTERVENTIONS_CANVAS_INTERVENTIONS_TEST_H_
