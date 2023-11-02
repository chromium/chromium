// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_FACTORY_H_
#define SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"

namespace base {
class FilePath;
}

namespace media {
class AudioDebugRecordingSession;
}

namespace audio {

COMPONENT_EXPORT(AUDIO_PUBLIC_CPP)
std::unique_ptr<media::AudioDebugRecordingSession>
CreateAudioDebugRecordingSession(
    const base::FilePath& debug_recording_file_path,
    mojo::PendingRemote<mojom::DebugRecording> debug_recording);

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_FACTORY_H_
