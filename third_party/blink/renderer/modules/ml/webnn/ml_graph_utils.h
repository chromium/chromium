// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class MLOperator;

// Return the operators in topological order by searching from the named
// output operands. It ensures operator 'j' appears before operator 'i' in the
// result, if 'i' depends on 'j'.
MODULES_EXPORT HeapVector<Member<const MLOperator>>*
GetOperatorsInTopologicalOrder(const MLNamedOperands& named_outputs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
