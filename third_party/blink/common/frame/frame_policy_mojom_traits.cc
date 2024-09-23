// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/frame/frame_policy_mojom_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::FramePolicyDataView, blink::FramePolicy>::Read(
    blink::mojom::FramePolicyDataView in,
    blink::FramePolicy* out) {
  // TODO(https://crbug.com/340618183): Add sanity check on enum values in
  // required_document_policy.
  return in.ReadSandboxFlags(&out->sandbox_flags) &&
         in.ReadContainerPolicy(&out->container_policy) &&
         in.ReadRequiredDocumentPolicy(&out->required_document_policy);
}

}  // namespace mojo
