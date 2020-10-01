// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"

namespace blink {

MediaSourceAttachmentSupplement::MediaSourceAttachmentSupplement() = default;

MediaSourceAttachmentSupplement::~MediaSourceAttachmentSupplement() = default;

void MediaSourceAttachmentSupplement::AddMainThreadAudioTrackToMediaElement(
    String /* id */,
    String /* kind */,
    String /* label */,
    String /* language */,
    bool /* enabled */) {
  // TODO(https::/crbug.com/878133): Remove this once cross-thread
  // implementation supports creation of worker-thread tracks.
  NOTIMPLEMENTED();
}

void MediaSourceAttachmentSupplement::AddMainThreadVideoTrackToMediaElement(
    String /* id */,
    String /* kind */,
    String /* label */,
    String /* language */,
    bool /* selected */) {
  // TODO(https::/crbug.com/878133): Remove this once cross-thread
  // implementation supports creation of worker-thread tracks.
  NOTIMPLEMENTED();
}

}  // namespace blink
