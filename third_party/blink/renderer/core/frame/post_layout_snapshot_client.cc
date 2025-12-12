// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/post_layout_snapshot_client.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

PostLayoutSnapshotClient::PostLayoutSnapshotClient(LocalFrame* frame) {
  if (frame) {
    frame->AddPostLayoutSnapshotClient(*this);
  }
}

void PostLayoutSnapshotClient::UpdateSnapshotForServiceAnimations() {
  UpdateSnapshot();
}

}  // namespace blink
