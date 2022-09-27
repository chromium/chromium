// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_STATE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_STATE_DATA_H_

#include "cc/input/scroll_state.h"

namespace blink {

// A wrapper around cc's structure to expose it to core.
struct ScrollStateData : public cc::ScrollStateData {
  ScrollStateData() : cc::ScrollStateData() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_STATE_DATA_H_
