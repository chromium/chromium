// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_BOX_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_BOX_OPTIONS_H_

namespace blink {

enum class ResizeObserverBoxOptions {
  kBorderBox,
  kContentBox,
  kDevicePixelContentBox
};
}

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_BOX_OPTIONS_H_
