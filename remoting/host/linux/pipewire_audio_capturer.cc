// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_audio_capturer.h"

#include <spa/param/audio/format-utils.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "library_loaders/remoting_libpipewire.h"
#include "remoting/base/logging.h"
#include "remoting/host/audio_silence_detector.h"
#include "remoting/host/linux/pipewire_utils.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

// The core object is started and destroyed on the caller's sequence, while
// HandleStreamProcess() and `callback` are called on the PipeWire thread.
// ScopedThreadLoopLock is used whenever the caller's sequence needs to access
// PipeWire resources to ensure thread safety.
class PipewireAudioCapturer::Core {
 public:
  Core();
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;
  ~Core();

  // Note: `callback` will be called on the PipeWire thread.
  bool Start(const PacketCapturedCallback& callback);

 private:
  static void OnStreamStateChanged(void* data,
                                   enum pw_stream_state old_state,
                                   enum pw_stream_state state,
                                   const char* error);
  static void OnStreamProcess(void* data);

  void HandleStreamProcess();

  const raw_ref<RemotingPipewireLoader> pw_{GetPipewireLoader()};
  AudioSilenceDetector silence_detector_;
  raw_ptr<pw_thread_loop> pw_main_loop_;
  raw_ptr<pw_context> pw_context_;
  raw_ptr<pw_core> pw_core_;
  raw_ptr<pw_stream> pw_stream_;
  spa_hook spa_stream_listener_;
  PacketCapturedCallback callback_;
};

DISABLE_CFI_DLSYM
PipewireAudioCapturer::Core::Core() : silence_detector_(0) {
  CHECK(EnsurePipewireInitialized()) << "PipeWire library is not initialized.";
}

DISABLE_CFI_DLSYM
PipewireAudioCapturer::Core::~Core() {
  if (pw_main_loop_) {
    pw_->pw_thread_loop_stop(pw_main_loop_);
  }
  {
    ScopedThreadLoopLock lock(pw_main_loop_);
    if (pw_stream_) {
      pw_->pw_stream_destroy(pw_stream_);
      pw_stream_ = nullptr;
    }
    if (pw_core_) {
      pw_->pw_core_disconnect(pw_core_);
      pw_core_ = nullptr;
    }
    if (pw_context_) {
      pw_->pw_context_destroy(pw_context_);
      pw_context_ = nullptr;
    }
  }
  if (pw_main_loop_) {
    pw_->pw_thread_loop_destroy(pw_main_loop_);
    pw_main_loop_ = nullptr;
  }
}

DISABLE_CFI_DLSYM
bool PipewireAudioCapturer::Core::Start(
    const PacketCapturedCallback& callback) {
  silence_detector_.Reset(AudioPacket::SAMPLING_RATE_48000,
                          AudioPacket::CHANNELS_STEREO);

  pw_main_loop_ =
      pw_->pw_thread_loop_new("crd-pipewire-audio-capturer", nullptr);
  if (!pw_main_loop_) {
    LOG(ERROR) << "Failed to create PipeWire thread loop.";
    return false;
  }

  pw_context_ = pw_->pw_context_new(pw_->pw_thread_loop_get_loop(pw_main_loop_),
                                    nullptr, 0);
  if (!pw_context_) {
    LOG(ERROR) << "Failed to create PipeWire context.";
    return false;
  }

  if (pw_->pw_thread_loop_start(pw_main_loop_) < 0) {
    LOG(ERROR) << "Failed to start PipeWire thread loop.";
    return false;
  }

  ScopedThreadLoopLock lock(pw_main_loop_);

  pw_core_ = pw_->pw_context_connect(pw_context_, nullptr, 0);
  if (!pw_core_) {
    LOG(ERROR) << "Failed to connect to PipeWire core.";
    return false;
  }

  VLOG(1) << "Connecting PipeWire Sink Stream";

  pw_properties* props = pw_->pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CLASS, "Audio/Sink",
      PW_KEY_NODE_NAME, "crd_audio_sink", PW_KEY_NODE_DESCRIPTION,
      "Chrome Remote Desktop Audio Sink", PW_KEY_NODE_VIRTUAL, "true", nullptr);

  pw_stream_ = pw_->pw_stream_new(pw_core_, "crd-audio-sink", props);

  if (!pw_stream_) {
    LOG(ERROR) << "Failed to create PipeWire stream.";
    return false;
  }

  static const struct pw_stream_events kStreamEvents = {
      .version = PW_VERSION_STREAM_EVENTS,
      .state_changed = &PipewireAudioCapturer::Core::OnStreamStateChanged,
      .process = &PipewireAudioCapturer::Core::OnStreamProcess,
  };

  pw_->pw_stream_add_listener(pw_stream_, &spa_stream_listener_, &kStreamEvents,
                              this);

  uint8_t buffer[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const struct spa_pod* params[1];

  struct spa_audio_info_raw info =
      SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16_LE, .rate = 48000,
                              .channels = 2);
  info.position[0] = SPA_AUDIO_CHANNEL_FL;
  info.position[1] = SPA_AUDIO_CHANNEL_FR;

  params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

  callback_ = callback;
  if (pw_->pw_stream_connect(
          pw_stream_, PW_DIRECTION_INPUT, PW_ID_ANY,
          static_cast<pw_stream_flags>(PW_STREAM_FLAG_MAP_BUFFERS |
                                       PW_STREAM_FLAG_RT_PROCESS),
          params, 1) < 0) {
    LOG(ERROR) << "Failed to connect PipeWire stream.";
    callback_.Reset();
    return false;
  }
  return true;
}

// static
DISABLE_CFI_DLSYM
void PipewireAudioCapturer::Core::OnStreamStateChanged(
    void* data,
    enum pw_stream_state old_state,
    enum pw_stream_state state,
    const char* error) {
  VLOG(1) << "PipeWire stream state changed: "
          << GetPipewireLoader().pw_stream_state_as_string(old_state) << " -> "
          << GetPipewireLoader().pw_stream_state_as_string(state);
  if (state == PW_STREAM_STATE_ERROR) {
    LOG(ERROR) << "PipeWire stream error: " << (error ? error : "unknown");
  }
}

// static
void PipewireAudioCapturer::Core::OnStreamProcess(void* data) {
  static_cast<PipewireAudioCapturer::Core*>(data)->HandleStreamProcess();
}

DISABLE_CFI_DLSYM
void PipewireAudioCapturer::Core::HandleStreamProcess() {
  // This is run on the PipeWire thread, so it's safe to call PipeWire functions
  // without locking.

  struct pw_buffer* b = pw_->pw_stream_dequeue_buffer(pw_stream_);
  if (!b) {
    return;
  }

  struct spa_buffer* buf = b->buffer;
  if (buf->n_datas == 0 || !buf->datas || !buf->datas[0].data) {
    // The buffer is not ready yet, so we put it back.
    pw_->pw_stream_queue_buffer(pw_stream_, b);
    return;
  }
  // In the PipeWire/SPA framework, interleaved formats (like the
  // SPA_AUDIO_FORMAT_S16_LE requested in Core::Start) store all audio channels
  // (e.g., Left and Right for stereo) multiplexed within a single memory block.
  // In this case, n_datas is 1, and all data is accessed via datas[0].
  DCHECK_EQ(buf->n_datas, 1u);
  struct spa_data& data = buf->datas[0];

  // The buffer is usually larger than the actual audio data. `chunk` tells us
  // where the actual audio data is and how large it is.
  struct spa_chunk* chunk = data.chunk;
  if (!chunk) {
    pw_->pw_stream_queue_buffer(pw_stream_, b);
    return;
  }
  uint32_t offset = chunk->offset;
  uint32_t size = chunk->size;
  // We write it this way instead of offset + size > data.maxsize to
  // avoid potential integer overflow.
  if (offset > data.maxsize || size > data.maxsize - offset) {
    LOG(ERROR) << "PipeWire buffer chunk is out of bounds.";
    pw_->pw_stream_queue_buffer(pw_stream_, b);
    return;
  }

  // SAFETY: This is required for wrapping a raw pointer from the PipeWire C API
  // into a base::span. PipeWire guarantees that the pointer (data.data) is
  // valid for at least `data.maxsize` bytes. We've already validated the offset
  // and size.
  auto data_span =
      UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(data.data), data.maxsize)
                         .subspan(offset, size));

  std::unique_ptr<AudioPacket> packet;
  if (!data_span.empty() && !silence_detector_.IsSilence(data_span)) {
    packet = std::make_unique<AudioPacket>();
    packet->add_data(data_span.data(), data_span.size());
    packet->set_encoding(AudioPacket::ENCODING_RAW);
    packet->set_sampling_rate(AudioPacket::SAMPLING_RATE_48000);
    packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
    packet->set_channels(AudioPacket::CHANNELS_STEREO);
  }

  // We are done with the buffer, so we put it back to PipeWire. Note that
  // `AudioPacket::add_data()` makes a deep copy of the audio data, so it's safe
  // to return the buffer to PipeWire before running the callback.
  pw_->pw_stream_queue_buffer(pw_stream_, b);

  if (packet) {
    callback_.Run(std::move(packet));
  }
}

PipewireAudioCapturer::PipewireAudioCapturer() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PipewireAudioCapturer::~PipewireAudioCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<PipewireAudioCapturer> PipewireAudioCapturer::Create() {
  if (!IsSupported()) {
    return nullptr;
  }
  return std::make_unique<PipewireAudioCapturer>();
}

// static
bool PipewireAudioCapturer::IsSupported() {
  return EnsurePipewireInitialized();
}

bool PipewireAudioCapturer::Start(const PacketCapturedCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_ = std::make_unique<Core>();
  // Bind a weak pointer of `this` to prevent callbacks from being posted after
  // `this` is destroyed.
  auto bound_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&PipewireAudioCapturer::OnPacketCaptured,
                          weak_ptr_factory_.GetWeakPtr(), callback));
  if (!core_->Start(std::move(bound_callback))) {
    core_.reset();
    return false;
  }
  return true;
}

void PipewireAudioCapturer::OnPacketCaptured(
    const PacketCapturedCallback& callback,
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback.Run(std::move(packet));
}

}  // namespace remoting
