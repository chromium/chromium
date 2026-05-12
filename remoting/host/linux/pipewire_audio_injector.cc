// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_audio_injector.h"

#include <pipewire/pipewire.h>
#include <pipewire/proxy.h>

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/fifo_buffer.h"
#include "remoting/base/in_memory_fifo_buffer.h"
#include "remoting/base/jitter_buffer.h"
#include "remoting/host/linux/pipewire_utils.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

namespace {
// The official specification for Opus in WebRTC (RFC 7587) mandates that the
// signaling must declare a clock rate of 48000 and 2 channels, even if the
// source is a 1-channel microphone.
constexpr uint32_t kAudioRate = 48000;
constexpr uint32_t kAudioChannels = 2;
constexpr size_t kBytesPerFrame = kAudioChannels * sizeof(int16_t);

// 256KB is about 1.3s of audio. This must be a power of two.
static constexpr size_t kJitterBufferCapacity = 256 * 1024;

static constexpr JitterBuffer::Config kJitterBufferConfig = {
    .frame_size = kBytesPerFrame,
    // 30ms of audio: 48000 * 4 * 0.03 = 5760 bytes.
    .max_starvation_bytes = 5760,
    // 150ms of audio: 192000 * 150 / 1000 = 28800 bytes.
    .max_latency_bytes = 28800,
    // 100ms of audio: 48000 * 0.1 * 4 = 19200 bytes.
    // TODO: crbug.com/502327751 - this is a rather high latency for real-time
    // audio streaming. See if we can bring it down to ~20ms.
    .minimum_threshold = 19200,
};
}  // namespace

// The core object is started and destroyed on the caller's sequence. `delegate`
// is called on the caller's sequence. Handle*() methods are called on the
// PipeWire thread. ScopedThreadLoopLock is used whenever the caller's sequence
// needs to access PipeWire resources to ensure thread safety.
class PipewireAudioInjector::Core {
 public:
  Core();
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;
  ~Core();

  bool Start(base::WeakPtr<Delegate> delegate);
  void InjectAudioPacket(std::unique_ptr<AudioPacket> packet);

 private:
  static void OnStreamStateChanged(void* data,
                                   enum pw_stream_state old_state,
                                   enum pw_stream_state state,
                                   const char* error);
  void HandleStreamStateChanged(enum pw_stream_state old_state,
                                enum pw_stream_state state,
                                const char* error);

  static void OnStreamProcess(void* data);
  void HandleStreamProcess();

  static void OnRegistryGlobal(void* data,
                               uint32_t id,
                               uint32_t permissions,
                               const char* type,
                               uint32_t version,
                               const struct spa_dict* props);
  void HandleRegistryGlobal(uint32_t id,
                            uint32_t permissions,
                            const char* type,
                            uint32_t version,
                            const struct spa_dict* props);

  static void OnRegistryGlobalRemove(void* data, uint32_t id);
  void HandleRegistryGlobalRemove(uint32_t id);

  const raw_ref<RemotingPipewireLoader> pw_{GetPipewireLoader()};
  base::RepeatingCallback<void(bool)> on_audio_injector_consumers_changed_cb_;

  ScopedPipewireMainLoop pw_main_loop_;
  ScopedPipewireContext pw_context_;
  ScopedPipewireCore pw_core_;
  ScopedPipewireProxy pw_registry_;
  ScopedPipewireStream pw_stream_;
  spa_hook spa_stream_listener_;
  spa_hook spa_registry_listener_;

  uint32_t stream_node_id_ = SPA_ID_INVALID;

  // Set of link IDs connected to this stream.
  std::set<uint32_t> active_links_;

  // Maps link ID to its output node ID. Used to track links discovered before
  // `stream_node_id_` is known. Cleared once `stream_node_id_` is known.
  std::map<uint32_t, uint32_t> pending_links_;

  std::unique_ptr<InMemoryFifoBufferWriter> audio_writer_;
  std::unique_ptr<JitterBuffer> audio_reader_;

  // Flag to defer `JitterBuffer::Clear()` from PipeWire's main thread to the
  // real-time processing thread (`HandleStreamProcess`) to ensure
  // thread-safety.
  std::atomic<bool> pending_clear_{false};
  std::atomic<bool> has_consumers_{false};
};

PipewireAudioInjector::Core::Core() {
  CHECK(EnsurePipewireInitialized()) << "PipeWire library is not initialized.";
  std::unique_ptr<InMemoryFifoBufferReader> reader;
  CHECK(CreateInMemoryFifoBuffer(kJitterBufferCapacity, audio_writer_, reader));
  audio_reader_ =
      std::make_unique<JitterBuffer>(kJitterBufferConfig, std::move(reader));
}

DISABLE_CFI_DLSYM
PipewireAudioInjector::Core::~Core() {
  if (!pw_main_loop_) {
    return;
  }
  pw_->pw_thread_loop_stop(pw_main_loop_.get());
  {
    ScopedThreadLoopLock lock(pw_main_loop_.get());
    pw_stream_ = nullptr;
    pw_registry_ = nullptr;
    pw_core_ = nullptr;
    pw_context_ = nullptr;
  }
  pw_main_loop_ = nullptr;
}

DISABLE_CFI_DLSYM
bool PipewireAudioInjector::Core::Start(base::WeakPtr<Delegate> delegate) {
  on_audio_injector_consumers_changed_cb_ = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&Delegate::OnAudioInjectorConsumersChanged,
                          delegate));
  pw_main_loop_.reset(pw_->pw_thread_loop_new("crd-audio-injector", nullptr));
  if (!pw_main_loop_) {
    LOG(ERROR) << "Failed to create PipeWire thread loop.";
    return false;
  }

  pw_context_.reset(pw_->pw_context_new(
      pw_->pw_thread_loop_get_loop(pw_main_loop_.get()), nullptr, 0));
  if (!pw_context_) {
    LOG(ERROR) << "Failed to create PipeWire context.";
    return false;
  }

  if (pw_->pw_thread_loop_start(pw_main_loop_.get()) < 0) {
    LOG(ERROR) << "Failed to start PipeWire thread loop.";
    return false;
  }

  ScopedThreadLoopLock lock(pw_main_loop_.get());

  pw_core_.reset(pw_->pw_context_connect(pw_context_.get(), nullptr, 0));
  if (!pw_core_) {
    LOG(ERROR) << "Failed to connect to PipeWire core.";
    return false;
  }

  pw_registry_.reset(reinterpret_cast<struct pw_proxy*>(
      pw_->pw_core_get_registry(pw_core_.get(), PW_VERSION_REGISTRY, 0)));
  if (!pw_registry_) {
    LOG(ERROR) << "Failed to get PipeWire registry.";
    return false;
  }

  static const struct pw_registry_events kRegistryEvents = {
      .version = PW_VERSION_REGISTRY_EVENTS,
      .global = &PipewireAudioInjector::Core::OnRegistryGlobal,
      .global_remove = &PipewireAudioInjector::Core::OnRegistryGlobalRemove,
  };

  pw_->pw_proxy_add_object_listener(pw_registry_.get(), &spa_registry_listener_,
                                    &kRegistryEvents, this);

  pw_properties* props = pw_->pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CLASS, "Audio/Source",
      PW_KEY_NODE_NAME, "crd_audio_input", PW_KEY_NODE_DESCRIPTION,
      "Chrome Remote Desktop Audio Input", PW_KEY_NODE_VIRTUAL, "true",
      // `crd_audio_sink` has monitor ports that mirror the input ports, so it
      // may be inadvertently selected as the default input device. We bump up
      // the priority of `crd_audio_input` here to ensure it takes precedence as
      // the default.
      PW_KEY_PRIORITY_SESSION, "1000", nullptr);

  static const struct pw_stream_events kStreamEvents = {
      .version = PW_VERSION_STREAM_EVENTS,
      .state_changed = &PipewireAudioInjector::Core::OnStreamStateChanged,
      .process = &PipewireAudioInjector::Core::OnStreamProcess,
  };

  pw_stream_ = CreatePipewireStream(pw_core_, props, &kStreamEvents, this,
                                    &spa_stream_listener_, PW_DIRECTION_OUTPUT,
                                    kAudioRate, kAudioChannels);
  if (!pw_stream_) {
    return false;
  }
  return true;
}

void PipewireAudioInjector::Core::InjectAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  if (!has_consumers_.load(std::memory_order_relaxed)) {
    return;
  }

  if (packet->encoding() != AudioPacket::ENCODING_RAW) {
    NOTIMPLEMENTED_LOG_ONCE()
        << "Unsupported audio encoding: " << packet->encoding();
    return;
  }

  if (packet->sampling_rate() != kAudioRate) {
    NOTIMPLEMENTED_LOG_ONCE()
        << "Unsupported audio sampling rate: " << packet->sampling_rate();
    return;
  }

  if (packet->channels() != kAudioChannels) {
    NOTIMPLEMENTED_LOG_ONCE()
        << "Unsupported audio channels: " << packet->channels();
    return;
  }

  for (const std::string& data : packet->data()) {
    if (data.size() % kBytesPerFrame != 0) {
      LOG(ERROR) << "Dropped misaligned audio data packet.";
      continue;
    }
    audio_writer_->Write(base::as_byte_span(data));
  }
}

// static
void PipewireAudioInjector::Core::OnStreamStateChanged(
    void* data,
    enum pw_stream_state old_state,
    enum pw_stream_state state,
    const char* error) {
  static_cast<Core*>(data)->HandleStreamStateChanged(old_state, state, error);
}

DISABLE_CFI_DLSYM
void PipewireAudioInjector::Core::HandleStreamStateChanged(
    enum pw_stream_state old_state,
    enum pw_stream_state state,
    const char* error) {
  VLOG(1) << "PipeWire stream state changed: "
          << pw_->pw_stream_state_as_string(old_state) << " -> "
          << pw_->pw_stream_state_as_string(state);

  if (state == PW_STREAM_STATE_ERROR) {
    LOG(ERROR) << "PipeWire stream error: " << (error ? error : "unknown");
    return;
  }

  if (state >= PW_STREAM_STATE_PAUSED && stream_node_id_ == SPA_ID_INVALID) {
    stream_node_id_ = pw_->pw_stream_get_node_id(pw_stream_.get());
    bool was_empty = active_links_.empty();

    for (auto const& [link_id, output_id] : pending_links_) {
      if (output_id == stream_node_id_) {
        active_links_.insert(link_id);
      }
    }
    pending_links_.clear();

    if (was_empty && !active_links_.empty()) {
      pending_clear_.store(true, std::memory_order_release);
      has_consumers_.store(true, std::memory_order_relaxed);
      on_audio_injector_consumers_changed_cb_.Run(true);
    }
  }
}

// static
void PipewireAudioInjector::Core::OnStreamProcess(void* data) {
  static_cast<Core*>(data)->HandleStreamProcess();
}

DISABLE_CFI_DLSYM
void PipewireAudioInjector::Core::HandleStreamProcess() {
  if (pending_clear_.exchange(false, std::memory_order_acquire)) {
    audio_reader_->Clear();
  }

  while (struct pw_buffer* b =
             pw_->pw_stream_dequeue_buffer(pw_stream_.get())) {
    struct spa_buffer* buf = b->buffer;
    if (!buf->datas[0].data) {
      pw_->pw_stream_queue_buffer(pw_stream_.get(), b);
      continue;
    }

    uint32_t stride = kBytesPerFrame;
    uint32_t n_frames = buf->datas[0].maxsize / stride;
    if (b->requested > 0) {
      n_frames = std::min(n_frames, static_cast<uint32_t>(b->requested));
    } else {
      // If PipeWire doesn't request a specific size, use a default period
      // (10ms).
      n_frames = std::min(n_frames, kAudioRate / 100u);
    }

    uint32_t target_bytes = n_frames * stride;

    // SAFETY: `buf->datas[0].data` is guaranteed by PipeWire to point to a
    // buffer of at least `buf->datas[0].maxsize` bytes.
    auto dst_span = UNSAFE_BUFFERS(base::span<uint8_t>(
        static_cast<uint8_t*>(buf->datas[0].data), buf->datas[0].maxsize));

    size_t bytes_read =
        audio_reader_->Read(dst_span.first(target_bytes)).value_or(0);

    if (bytes_read < target_bytes) {
      // Fill the rest of the buffer with silence.
      std::ranges::fill(dst_span.subspan(bytes_read, target_bytes - bytes_read),
                        0);
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = target_bytes;

    pw_->pw_stream_queue_buffer(pw_stream_.get(), b);

    if (bytes_read < target_bytes) {
      break;
    }
  }
}

// static
void PipewireAudioInjector::Core::OnRegistryGlobal(
    void* data,
    uint32_t id,
    uint32_t permissions,
    const char* type,
    uint32_t version,
    const struct spa_dict* props) {
  static_cast<Core*>(data)->HandleRegistryGlobal(id, permissions, type, version,
                                                 props);
}

DISABLE_CFI_DLSYM
void PipewireAudioInjector::Core::HandleRegistryGlobal(
    uint32_t id,
    uint32_t permissions,
    const char* type,
    uint32_t version,
    const struct spa_dict* props) {
  if (!type || !props) {
    return;
  }
  std::string_view type_view(type);
  if (type_view != PW_TYPE_INTERFACE_Link) {
    return;
  }
  const char* output_node = spa_dict_lookup(props, PW_KEY_LINK_OUTPUT_NODE);
  uint32_t output_id;
  if (!output_node || !base::StringToUint(output_node, &output_id)) {
    return;
  }
  if (stream_node_id_ == SPA_ID_INVALID) {
    pending_links_[id] = output_id;
    return;
  }
  if (output_id != stream_node_id_) {
    return;
  }
  if (active_links_.empty()) {
    pending_clear_.store(true, std::memory_order_release);
    has_consumers_.store(true, std::memory_order_relaxed);
    on_audio_injector_consumers_changed_cb_.Run(true);
  }
  active_links_.insert(id);
}

// static
void PipewireAudioInjector::Core::OnRegistryGlobalRemove(void* data,
                                                         uint32_t id) {
  static_cast<Core*>(data)->HandleRegistryGlobalRemove(id);
}

void PipewireAudioInjector::Core::HandleRegistryGlobalRemove(uint32_t id) {
  pending_links_.erase(id);
  if (active_links_.erase(id) > 0 && active_links_.empty()) {
    has_consumers_.store(false, std::memory_order_relaxed);
    pending_clear_.store(true, std::memory_order_release);
    on_audio_injector_consumers_changed_cb_.Run(false);
  }
}

PipewireAudioInjector::PipewireAudioInjector() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PipewireAudioInjector::~PipewireAudioInjector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool PipewireAudioInjector::IsSupported() {
  return EnsurePipewireInitialized();
}

// static
std::unique_ptr<PipewireAudioInjector> PipewireAudioInjector::Create(
    std::unique_ptr<FifoBufferReader> audio_reader) {
  if (!IsSupported()) {
    return nullptr;
  }
  return std::make_unique<PipewireAudioInjector>();
}

bool PipewireAudioInjector::Start(base::WeakPtr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_ = std::make_unique<Core>();
  if (!core_->Start(delegate)) {
    core_.reset();
    return false;
  }
  return true;
}

void PipewireAudioInjector::InjectAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_) << "Start() has not been called.";

  core_->InjectAudioPacket(std::move(packet));
}

base::WeakPtr<protocol::AudioStub> PipewireAudioInjector::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
