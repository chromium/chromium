// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLGraphBuilder;

class MLOperator final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MLOperator(MLGraphBuilder* graph_builder);

  MLOperator(const MLOperator&) = delete;
  MLOperator& operator=(const MLOperator&) = delete;

  ~MLOperator() override;

  void Trace(Visitor* visitor) const override;

 private:
  Member<MLGraphBuilder> graph_builder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERATOR_H_
