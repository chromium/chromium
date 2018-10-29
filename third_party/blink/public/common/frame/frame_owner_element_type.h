// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_OWNER_ELEMENT_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_OWNER_ELEMENT_TYPE_H_

#include "third_party/blink/common/common_export.h"

namespace blink {

// The type of the frame owner element for a frame. In cross-process frames,
// this would be the type of the HTMLFrameOwnerElement for the remote frame in
// the parent process.
enum class FrameOwnerElementType {
  kNone = 0 /* For a main frame */,
  kIframe,
  kObject,
  kEmbed,
  kFrame,
  kPortal,
  kMaxValue = kPortal
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_OWNER_ELEMENT_TYPE_H_
