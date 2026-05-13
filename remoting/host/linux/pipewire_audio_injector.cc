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

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/fifo_buffer.h"
#include "remoting/host/linux/pipewire_utils.h"
#include "remoting/protocol/audio_sample_info.h"

namespace remoting {

namespace {
// The official specification for Opus in WebRTC (RFC 7587) mandates that the
// signaling must declare a clock rate of 48000 and 2 channels, even if the
// source is a 1-channel microphone.
constexpr uint32_t kAudioRate = 48000;
constexpr uint32_t kAudioChannels = 2;
constexpr size_t kBytesPerFrame = kAudioChannels * sizeof(int16_t);

}  // namespace

// The core object is started and destroyed on the caller's sequence. `delegate`
// is called on the caller's sequence. Handle*() methods are called on the
// PipeWire thread. ScopedThreadLoopLock is used whenever the caller's sequence
// needs to access PipeWire resources to ensure thread safety.
class PipewireAudioInjector::Core {
 public:
  explicit Core(std::unique_ptr<FifoBufferReader> reader);
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;
  ~Core();

  bool Start(base::WeakPtr<Delegate> delegate);
  void SetFormatReady(bool ready);
  void ClearBuffer(base::OnceClosure done);

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

  std::unique_ptr<FifoBufferReader> audio_reader_;

  // Flag to defer `JitterBuffer::Clear()` from PipeWire's main thread to the
  // real-time processing thread (`HandleStreamProcess`) to ensure
  // thread-safety.
  std::atomic<bool> pending_clear_{false};

  std::atomic<bool> format_ready_{false};
  // Guarded by `callback_lock_` to synchronize the main thread with PipeWire's
  // independent real-time data loop. While `pw_main_loop_` locks control
  // events, `HandleStreamProcess` executes on a dedicated streaming thread that
  // does not acquire the main loop lock, making explicit synchronization
  // necessary.
  base::Lock callback_lock_;
  base::OnceClosure on_buffer_cleared_cb_ GUARDED_BY(callback_lock_);
};

PipewireAudioInjector::Core::Core(std::unique_ptr<FifoBufferReader> reader) {
  CHECK(EnsurePipewireInitialized()) << "PipeWire library is not initialized.";
  audio_reader_ = std::move(reader);
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
    LOG(ERROR) << "Failed to create PipeWire stream.";
    return false;
  }
  return true;
}

void PipewireAudioInjector::Core::SetFormatReady(bool ready) {
  format_ready_.store(ready, std::memory_order_relaxed);
}

void PipewireAudioInjector::Core::ClearBuffer(base::OnceClosure done) {
  ScopedThreadLoopLock lock(pw_main_loop_.get());
  base::OnceClosure old_on_buffer_cleared_cb;
  {
    base::AutoLock auto_lock(callback_lock_);
    old_on_buffer_cleared_cb = std::move(on_buffer_cleared_cb_);
    on_buffer_cleared_cb_ = std::move(done);
  }
  if (old_on_buffer_cleared_cb) {
    std::move(old_on_buffer_cleared_cb).Run();
  }
  pending_clear_.store(true, std::memory_order_release);
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
    base::OnceClosure on_buffer_cleared_cb;
    {
      base::AutoLock auto_lock(callback_lock_);
      on_buffer_cleared_cb = std::move(on_buffer_cleared_cb_);
    }
    if (on_buffer_cleared_cb) {
      std::move(on_buffer_cleared_cb).Run();
    }
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

    size_t bytes_read = 0;
    if (format_ready_.load(std::memory_order_relaxed)) {
      bytes_read =
          audio_reader_->Read(dst_span.first(target_bytes)).value_or(0);
    }

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
    pending_clear_.store(true, std::memory_order_release);
    on_audio_injector_consumers_changed_cb_.Run(false);
  }
}

PipewireAudioInjector::PipewireAudioInjector(
    std::unique_ptr<FifoBufferReader> audio_reader)
    : audio_reader_(std::move(audio_reader)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK(audio_reader_);
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
  if (!audio_reader) {
    LOG(ERROR) << "Cannot create audio injector without an audio reader.";
    return nullptr;
  }
  return std::make_unique<PipewireAudioInjector>(std::move(audio_reader));
}

bool PipewireAudioInjector::Start(base::WeakPtr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (core_) {
    LOG(ERROR) << "Audio injector is already started.";
    return false;
  }

  core_ = std::make_unique<Core>(std::move(audio_reader_));
  core_->SetFormatReady(format_ready_);
  if (!core_->Start(delegate)) {
    core_.reset();
    return false;
  }
  return true;
}

void PipewireAudioInjector::SetSampleInfo(const protocol::AudioSampleInfo& info,
                                          base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear the incoming buffer to discard stale samples in the old format.
  // If the injector is active, clearing is deferred to the streaming thread.
  // The buffer will be filled with samples in the new format after `done` is
  // called.
  format_ready_ =
      info.sampling_rate == kAudioRate && info.channels == kAudioChannels;
  if (!format_ready_) {
    LOG(ERROR) << "Unsupported audio sample info: rate=" << info.sampling_rate
               << ", channels=" << static_cast<int>(info.channels);
  }

  if (core_) {
    core_->SetFormatReady(format_ready_);
    base::OnceClosure wrapped_done =
        done ? base::BindPostTaskToCurrentDefault(std::move(done))
             : base::DoNothing();
    core_->ClearBuffer(std::move(wrapped_done));
  } else {
    if (audio_reader_) {
      audio_reader_->Clear();
    }
    if (done) {
      // TODO: crbug.com/509659010 - Add a parameter to the done callback to
      // indicate whether the injector supports the new format, so that the
      // writer can stop writing.
      std::move(done).Run();
    }
  }
}

base::WeakPtr<AudioInjector> PipewireAudioInjector::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace remoting
