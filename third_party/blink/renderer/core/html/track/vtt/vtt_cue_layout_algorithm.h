// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_CUE_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_CUE_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class VTTCueBox;

// VttCueLayoutAlgorithm is responsible to do step 10 of
// https://w3c.github.io/webvtt/#apply-webvtt-cue-settings .
//
// This class is used in a ResizeObserver callback for VTTCueBox.
class VttCueLayoutAlgorithm {
  STACK_ALLOCATED();

 public:
  explicit VttCueLayoutAlgorithm(VTTCueBox& cue);

  void Layout();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_CUE_LAYOUT_ALGORITHM_H_
