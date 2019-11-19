// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_WINDOW_FEATURES_CONVERTER_H_
#define CONTENT_PUBLIC_RENDERER_WINDOW_FEATURES_CONVERTER_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "third_party/blink/public/web/web_window_features.h"

namespace content {

CONTENT_EXPORT blink::mojom::WindowFeaturesPtr
ConvertWebWindowFeaturesToMojoWindowFeatures(
    const blink::WebWindowFeatures& web_window_features);

CONTENT_EXPORT blink::WebWindowFeatures
ConvertMojoWindowFeaturesToWebWindowFeatures(
    const blink::mojom::WindowFeatures& window_features);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_WINDOW_FEATURES_CONVERTER_H_
