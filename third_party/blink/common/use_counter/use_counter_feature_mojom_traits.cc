// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/use_counter/use_counter_feature_mojom_traits.h"
namespace mojo {

bool StructTraits<
    blink::mojom::UseCounterFeatureDataView,
    blink::UseCounterFeature>::Read(blink::mojom::UseCounterFeatureDataView in,
                                    blink::UseCounterFeature* out) {
  return out->SetTypeAndValue(in.type(), in.value());
}

}  // namespace mojo
