// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_INITIAL_VIEWPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_INITIAL_VIEWPORT_H_

#include "third_party/blink/renderer/core/css/media_values_dynamic.h"

namespace blink {

class CORE_EXPORT MediaValuesInitialViewport final : public MediaValuesDynamic {
 public:
  explicit MediaValuesInitialViewport(LocalFrame&);

  double ViewportWidth() const override;
  double ViewportHeight() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_VALUES_INITIAL_VIEWPORT_H_
