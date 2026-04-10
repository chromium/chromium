// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pulse_audio_capturer.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
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
void PulseAudioCapturer::InitializePipeReader(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::FilePath& pipe_name) {
  scoped_refptr<AudioPipeReader> pipe_reader;
  if (!pipe_name.empty()) {
    pipe_reader = AudioPipeReader::Create(task_runner, pipe_name);
  }
  g_pulseaudio_pipe_sink_reader.Get() = pipe_reader;
}

PulseAudioCapturer::PulseAudioCapturer(
    scoped_refptr<AudioPipeReader> pipe_reader)
    : pipe_reader_(pipe_reader), silence_detector_(0) {}

PulseAudioCapturer::~PulseAudioCapturer() {
  pipe_reader_->RemoveObserver(this);
}

bool PulseAudioCapturer::Start(const PacketCapturedCallback& callback) {
  callback_ = callback;
  silence_detector_.Reset(AudioPipeReader::kSamplingRate,
                          AudioPipeReader::kChannels);
  pipe_reader_->AddObserver(this);
  return true;
}

void PulseAudioCapturer::OnDataRead(
    scoped_refptr<base::RefCountedString> data) {
  DCHECK(!callback_.is_null());

  if (silence_detector_.IsSilence(base::as_byte_span(data->as_string()))) {
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

bool PulseAudioCapturer::IsSupported() {
  return g_pulseaudio_pipe_sink_reader.Get().get() != nullptr;
}

std::unique_ptr<AudioCapturer> PulseAudioCapturer::Create() {
  scoped_refptr<AudioPipeReader> reader = g_pulseaudio_pipe_sink_reader.Get();
  if (!reader.get()) {
    return nullptr;
  }
  return std::make_unique<PulseAudioCapturer>(reader);
}

}  // namespace remoting
