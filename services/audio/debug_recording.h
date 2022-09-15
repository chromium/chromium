// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_DEBUG_RECORDING_H_
#define SERVICES_AUDIO_DEBUG_RECORDING_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/debug_recording.mojom.h"

namespace media {
class AecdumpRecordingManager;
class AudioManager;
enum class AudioDebugRecordingStreamType;
}  // namespace media

namespace audio {

// Implementation for controlling audio debug recording.
class DebugRecording : public mojom::DebugRecording {
 public:
  DebugRecording(mojo::PendingReceiver<mojom::DebugRecording> receiver,
                 media::AudioManager* audio_manager,
                 media::AecdumpRecordingManager* aecdump_recording_manager);

  DebugRecording(const DebugRecording&) = delete;
  DebugRecording& operator=(const DebugRecording&) = delete;

  // Disables audio debug recording if Enable() was called before.
  ~DebugRecording() override;

  // Enables audio debug recording.
  void Enable(mojo::PendingRemote<mojom::DebugRecordingFileProvider>
                  file_provider) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DebugRecordingTest,
                           CreateFileCallsFileProviderCreateFile);
  // Called on binding connection error.
  void Disable();

  void CreateWavFile(
      media::AudioDebugRecordingStreamType stream_type,
      uint32_t id,
      mojom::DebugRecordingFileProvider::CreateWavFileCallback reply_callback);
  void CreateAecdumpFile(
      uint32_t id,
      mojom::DebugRecordingFileProvider::CreateAecdumpFileCallback
          reply_callback);
  bool IsEnabled();

  const raw_ptr<media::AudioManager> audio_manager_;
  const raw_ptr<media::AecdumpRecordingManager> aecdump_recording_manager_;
  mojo::Receiver<mojom::DebugRecording> receiver_;
  mojo::Remote<mojom::DebugRecordingFileProvider> file_provider_;

  base::WeakPtrFactory<DebugRecording> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_DEBUG_RECORDING_H_
