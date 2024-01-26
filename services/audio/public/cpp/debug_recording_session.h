// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_H_
#define SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_H_

#include <memory>

#include "base/component_export.h"
#include "media/audio/audio_debug_recording_helper.h"
#include "media/audio/audio_debug_recording_session.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"

namespace base {
class FilePath;
}

namespace audio {

// Client class for using mojom::DebugRecording interface. This class owns
// mojom::DebugRecordingFileProvider implementation, therefore owners of this
// class' instances need permission to create files in |file_name_base| path
// passed in constructor in order to start debug recording. If file creation
// fails, debug recording will silently not start.
class COMPONENT_EXPORT(AUDIO_PUBLIC_CPP) DebugRecordingSession
    : public media::AudioDebugRecordingSession {
 public:
  class COMPONENT_EXPORT(AUDIO_PUBLIC_CPP) DebugRecordingFileProvider
      : public mojom::DebugRecordingFileProvider {
   public:
    DebugRecordingFileProvider(
        mojo::PendingReceiver<mojom::DebugRecordingFileProvider> receiver,
        const base::FilePath& file_name_base);

    DebugRecordingFileProvider(const DebugRecordingFileProvider&) = delete;
    DebugRecordingFileProvider& operator=(const DebugRecordingFileProvider&) =
        delete;

    ~DebugRecordingFileProvider() override;

    // Creates file with name "|file_name_base_|.<stream_type_str>.|id|.wav",
    // where <stream_type_str> is "input" or "output" depending on |stream_type|
    // value.
    void CreateWavFile(media::AudioDebugRecordingStreamType stream_type,
                       uint32_t id,
                       CreateWavFileCallback reply_callback) override;

    // Creates file with name "|file_name_base_|.|id|.aecdump".
    void CreateAecdumpFile(uint32_t id,
                           CreateAecdumpFileCallback reply_callback) override;

   private:
    mojo::Receiver<mojom::DebugRecordingFileProvider> receiver_;
    base::FilePath file_name_base_;
  };

  DebugRecordingSession(
      const base::FilePath& file_name_base,
      mojo::PendingRemote<mojom::DebugRecording> debug_recording);

  DebugRecordingSession(const DebugRecordingSession&) = delete;
  DebugRecordingSession& operator=(const DebugRecordingSession&) = delete;

  ~DebugRecordingSession() override;

 private:
  std::unique_ptr<DebugRecordingFileProvider> file_provider_;
  mojo::Remote<mojom::DebugRecording> debug_recording_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_DEBUG_RECORDING_SESSION_H_
