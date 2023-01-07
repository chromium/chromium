// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_data_transfer_list.h"

#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"

namespace blink {

const void* const AudioDataTransferList::kTransferListKey = nullptr;

void AudioDataTransferList::FinalizeTransfer(ExceptionState& exception_state) {
  for (AudioData* audio_data : audio_data_collection)
    audio_data->close();
}

void AudioDataTransferList::Trace(Visitor* visitor) const {
  visitor->Trace(audio_data_collection);
}

}  // namespace blink
