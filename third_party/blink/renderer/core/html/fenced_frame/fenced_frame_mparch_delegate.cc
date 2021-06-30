// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_mparch_delegate.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

FencedFrameMPArchDelegate::FencedFrameMPArchDelegate(
    HTMLFencedFrameElement* outer_element)
    : HTMLFencedFrameElement::FencedFrameDelegate(outer_element) {
  DCHECK_EQ(features::kFencedFramesImplementationTypeParam.Get(),
            features::FencedFramesImplementationType::kMPArch);
}

void FencedFrameMPArchDelegate::DidGetInserted() {
  RemoteFrame* remote_frame =
      GetElement().GetDocument().GetFrame()->Client()->CreateFencedFrame(
          &GetElement());
  DCHECK_EQ(remote_frame, GetElement().ContentFrame());
}

void FencedFrameMPArchDelegate::Navigate(const KURL& url) {}

}  // namespace blink
