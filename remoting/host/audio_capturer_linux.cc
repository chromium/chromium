// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_linux.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

namespace {

base::LazyInstance<scoped_refptr<AudioPipeReader>>::Leaky
    g_pulseaudio_pipe_sink_reader = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// TODO(wez): Remove this and have the DesktopEnvironmentFactory own the
// AudioPipeReader rather than having it process-global.
// See crbug.com/161373 and crbug.com/104544.
void AudioCapturerLinux::InitializePipeReader(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::FilePath& pipe_name) {
  scoped_refptr<AudioPipeReader> pipe_reader;
  if (!pipe_name.empty()) {
    pipe_reader = AudioPipeReader::Create(task_runner, pipe_name);
  }
  g_pulseaudio_pipe_sink_reader.Get() = pipe_reader;
}

AudioCapturerLinux::AudioCapturerLinux(
    scoped_refptr<AudioPipeReader> pipe_reader)
    : pipe_reader_(pipe_reader), silence_detector_(0) {}

AudioCapturerLinux::~AudioCapturerLinux() {
  pipe_reader_->RemoveObserver(this);
}

bool AudioCapturerLinux::Start(const PacketCapturedCallback& callback) {
  callback_ = callback;
  silence_detector_.Reset(AudioPipeReader::kSamplingRate,
                          AudioPipeReader::kChannels);
  pipe_reader_->AddObserver(this);
  return true;
}

void AudioCapturerLinux::OnDataRead(
    scoped_refptr<base::RefCountedString> data) {
  DCHECK(!callback_.is_null());

  if (silence_detector_.IsSilence(
          // TODO(danakj): This cast can cause UB, we should copy into integers
          // or pass it as a byte span.
          reinterpret_cast<const int16_t*>(data->as_string().data()),
          data->as_string().size() / sizeof(int16_t) /
              AudioPipeReader::kChannels)) {
    return;
  }

  auto packet = std::make_unique<AudioPacket>();
  packet->add_data(data->as_string());
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(AudioPipeReader::kSamplingRate);
  packet->set_bytes_per_sample(AudioPipeReader::kBytesPerSample);
  packet->set_channels(AudioPipeReader::kChannels);
  callback_.Run(std::move(packet));
}

bool AudioCapturer::IsSupported() {
  return g_pulseaudio_pipe_sink_reader.Get().get() != nullptr;
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  scoped_refptr<AudioPipeReader> reader = g_pulseaudio_pipe_sink_reader.Get();
  if (!reader.get()) {
    return nullptr;
  }
  return base::WrapUnique(new AudioCapturerLinux(reader));
}

}  // namespace remoting
