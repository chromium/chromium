// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class PrivateAggregationDebugModeOptions;
class PrivateAggregationHistogramContribution;
class ScriptState;

class MODULES_EXPORT PrivateAggregation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PrivateAggregation();
  ~PrivateAggregation() override;

  void Trace(Visitor*) const override;

  // PrivateAggregation IDL
  void contributeToHistogram(ScriptState*,
                             const PrivateAggregationHistogramContribution*,
                             ExceptionState&);
  void enableDebugMode(ScriptState*, ExceptionState&);
  void enableDebugMode(ScriptState*,
                       const PrivateAggregationDebugModeOptions*,
                       ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_PRIVATE_AGGREGATION_H_
