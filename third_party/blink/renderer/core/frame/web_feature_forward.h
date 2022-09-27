// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FEATURE_FORWARD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FEATURE_FORWARD_H_

#include <cstdint>

namespace blink {

// This header just forward-declares WebFeature for the blink namespace.
// Including the actual file that defines the WebFeature enum is heavy on the
// compiler, so it should be avoided when possible. Those who *do* need the
// definition, though, need to include WebFeature.h instead.

namespace mojom {
enum class WebFeature : int32_t;
}  // namespace mojom
using WebFeature = mojom::WebFeature;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FEATURE_FORWARD_H_
