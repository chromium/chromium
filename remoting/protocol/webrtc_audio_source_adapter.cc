// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_audio_source_adapter.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_source.h"

namespace remoting::protocol {

namespace {

static const int kChannels = 2;
static const int kBytesPerSample = 2;

// Frame size expected by webrtc::AudioTrackSinkInterface.
static constexpr base::TimeDelta kAudioFrameDuration = base::Milliseconds(10);

// Notify all audio sinks about a new audio frame.
void NotifyAudioSinks(
    base::ObserverList<webrtc::AudioTrackSinkInterface>::Unchecked& audio_sinks,
    base::span<const uint8_t> frame,
    int sampling_rate,
    size_t samples_per_frame) {
  for (auto& observer : audio_sinks) {
    observer.OnData(frame.data(), kBytesPerSample * 8, sampling_rate, kChannels,
                    samples_per_frame);
  }
}

}  // namespace

class WebrtcAudioSourceAdapter::Core {
 public:
  Core();
  ~Core();

  void Start(std::unique_ptr<AudioSource> audio_source);
  void Pause(bool pause);
  void AddSink(webrtc::AudioTrackSinkInterface* sink);
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink);

 private:
  void OnAudioPacket(std::unique_ptr<AudioPacket> packet);

  std::unique_ptr<AudioSource> audio_source_;

  bool paused_ = false;

  int sampling_rate_ = 0;

  // webrtc::AudioTrackSinkInterface expects to get audio in 10ms frames (see
  // kAudioFrameDuration). AudioSource may generate AudioPackets for time
  // intervals that are not multiple of 10ms. In that case the left-over samples
  // are kept in |partial_frame_| until the next AudioPacket is captured by the
  // AudioSource.
  std::vector<uint8_t> partial_frame_;

  base::ObserverList<webrtc::AudioTrackSinkInterface>::Unchecked audio_sinks_;
  base::Lock audio_sinks_lock_;

  THREAD_CHECKER(thread_checker_);
};

WebrtcAudioSourceAdapter::Core::Core() {
  DETACH_FROM_THREAD(thread_checker_);
}

WebrtcAudioSourceAdapter::Core::~Core() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void WebrtcAudioSourceAdapter::Core::Start(
    std::unique_ptr<AudioSource> audio_source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  audio_source_ = std::move(audio_source);
  audio_source_->Start(
      base::BindRepeating(&Core::OnAudioPacket, base::Unretained(this)));
}

void WebrtcAudioSourceAdapter::Core::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  paused_ = pause;
}

void WebrtcAudioSourceAdapter::Core::AddSink(
    webrtc::AudioTrackSinkInterface* sink) {
  // Can be called on any thread.
  base::AutoLock lock(audio_sinks_lock_);
  audio_sinks_.AddObserver(sink);
}

void WebrtcAudioSourceAdapter::Core::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  // Can be called on any thread.
  base::AutoLock lock(audio_sinks_lock_);
  audio_sinks_.RemoveObserver(sink);
}

void WebrtcAudioSourceAdapter::Core::OnAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (paused_) {
    return;
  }

  DCHECK_EQ(packet->channels(), kChannels);
  DCHECK_EQ(packet->bytes_per_sample(), kBytesPerSample);

  if (sampling_rate_ != packet->sampling_rate()) {
    sampling_rate_ = packet->sampling_rate();
    partial_frame_.clear();
  }

  const size_t samples_per_frame =
      (kAudioFrameDuration * sampling_rate_).InSeconds();
  const size_t bytes_per_frame =
      kBytesPerSample * kChannels * samples_per_frame;

  base::span<const uint8_t> input_data = base::as_byte_span(packet->data(0));

  base::AutoLock lock(audio_sinks_lock_);

  // Stage 1: Fill and send |partial_frame_|.
  if (!partial_frame_.empty()) {
    const size_t needed_bytes = bytes_per_frame - partial_frame_.size();
    const size_t copy_bytes = std::min(needed_bytes, input_data.size());

    partial_frame_.insert(partial_frame_.end(), input_data.begin(),
                          input_data.begin() + copy_bytes);
    input_data = input_data.subspan(copy_bytes);

    if (partial_frame_.size() == bytes_per_frame) {
      NotifyAudioSinks(audio_sinks_, base::span<const uint8_t>(partial_frame_),
                       sampling_rate_, samples_per_frame);
      partial_frame_.clear();
    }
  }

  // Stage 2: Processing of |full_frames|.
  const size_t full_frames = input_data.size() / bytes_per_frame;
  for (size_t i = 0; i < full_frames; ++i) {
    const auto frame = input_data.subspan(i * bytes_per_frame, bytes_per_frame);
    NotifyAudioSinks(audio_sinks_, frame, sampling_rate_, samples_per_frame);
  }

  // Stage 3: Save remaining data.
  const size_t processed_bytes = full_frames * bytes_per_frame;
  const auto remaining = input_data.subspan(processed_bytes);
  partial_frame_.insert(partial_frame_.end(), remaining.begin(),
                        remaining.end());
}

WebrtcAudioSourceAdapter::WebrtcAudioSourceAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner)
    : audio_task_runner_(std::move(audio_task_runner)), core_(new Core()) {}

WebrtcAudioSourceAdapter::~WebrtcAudioSourceAdapter() {
  audio_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void WebrtcAudioSourceAdapter::Start(
    std::unique_ptr<AudioSource> audio_source) {
  audio_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get()),
                                std::move(audio_source)));
}

void WebrtcAudioSourceAdapter::Pause(bool pause) {
  audio_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Pause, base::Unretained(core_.get()), pause));
}

WebrtcAudioSourceAdapter::SourceState WebrtcAudioSourceAdapter::state() const {
  return kLive;
}

bool WebrtcAudioSourceAdapter::remote() const {
  return false;
}

void WebrtcAudioSourceAdapter::RegisterAudioObserver(AudioObserver* observer) {}

void WebrtcAudioSourceAdapter::UnregisterAudioObserver(
    AudioObserver* observer) {}

void WebrtcAudioSourceAdapter::AddSink(webrtc::AudioTrackSinkInterface* sink) {
  core_->AddSink(sink);
}
void WebrtcAudioSourceAdapter::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  core_->RemoveSink(sink);
}

void WebrtcAudioSourceAdapter::RegisterObserver(
    webrtc::ObserverInterface* observer) {}
void WebrtcAudioSourceAdapter::UnregisterObserver(
    webrtc::ObserverInterface* observer) {}

}  // namespace remoting::protocol
