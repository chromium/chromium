// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_destination_handler.h"

namespace blink {

AudioDestinationHandler::AudioDestinationHandler(AudioNode& node)
    : AudioHandler(kNodeTypeDestination, node, 0) {
  AddInput();
}

AudioDestinationHandler::~AudioDestinationHandler() {
  DCHECK(!IsInitialized());
}

}  // namespace blink
