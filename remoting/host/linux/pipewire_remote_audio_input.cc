// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_remote_audio_input.h"

#include <pipewire/pipewire.h>
#include <pipewire/proxy.h>

#include <map>
#include <set>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/pipewire_utils.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

namespace {
// TODO: crbug.com/502327751 - Check to make sure these are valid. If they don't
// match the values from the AudioPacket then we may need to change it using
// pw_stream_update_params().
constexpr uint32_t kAudioRate = 48000;
constexpr uint32_t kAudioChannels = 1;
}  // namespace

// The core object is started and destroyed on the caller's sequence. `delegate`
// is called on the caller's sequence. Handle*() methods are called on the
// PipeWire thread. ScopedThreadLoopLock is used whenever the caller's sequence
// needs to access PipeWire resources to ensure thread safety.
class PipewireRemoteAudioInput::Core {
 public:
  Core();
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;
  ~Core();

  bool Start(base::WeakPtr<Delegate> delegate);
  void OnAudioPacket(std::unique_ptr<AudioPacket> packet);

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
  base::RepeatingCallback<void(bool)> on_active_consumers_changed_cb_;

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
};

PipewireRemoteAudioInput::Core::Core() {
  CHECK(EnsurePipewireInitialized()) << "PipeWire library is not initialized.";
}

DISABLE_CFI_DLSYM
PipewireRemoteAudioInput::Core::~Core() {
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
bool PipewireRemoteAudioInput::Core::Start(base::WeakPtr<Delegate> delegate) {
  on_active_consumers_changed_cb_ = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&Delegate::OnActiveConsumersChanged, delegate));
  pw_main_loop_.reset(
      pw_->pw_thread_loop_new("crd-remote-audio-input", nullptr));
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
      .global = &PipewireRemoteAudioInput::Core::OnRegistryGlobal,
      .global_remove = &PipewireRemoteAudioInput::Core::OnRegistryGlobalRemove,
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
      .state_changed = &PipewireRemoteAudioInput::Core::OnStreamStateChanged,
      .process = &PipewireRemoteAudioInput::Core::OnStreamProcess,
  };

  pw_stream_ = CreatePipewireStream(pw_core_, props, &kStreamEvents, this,
                                    &spa_stream_listener_, PW_DIRECTION_OUTPUT,
                                    kAudioRate, kAudioChannels);
  if (!pw_stream_) {
    return false;
  }
  return true;
}

void PipewireRemoteAudioInput::Core::OnAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void PipewireRemoteAudioInput::Core::OnStreamStateChanged(
    void* data,
    enum pw_stream_state old_state,
    enum pw_stream_state state,
    const char* error) {
  static_cast<Core*>(data)->HandleStreamStateChanged(old_state, state, error);
}

DISABLE_CFI_DLSYM
void PipewireRemoteAudioInput::Core::HandleStreamStateChanged(
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
      on_active_consumers_changed_cb_.Run(true);
    }
  }
}

// static
void PipewireRemoteAudioInput::Core::OnStreamProcess(void* data) {
  static_cast<Core*>(data)->HandleStreamProcess();
}

DISABLE_CFI_DLSYM
void PipewireRemoteAudioInput::Core::HandleStreamProcess() {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void PipewireRemoteAudioInput::Core::OnRegistryGlobal(
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
void PipewireRemoteAudioInput::Core::HandleRegistryGlobal(
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
    on_active_consumers_changed_cb_.Run(true);
  }
  active_links_.insert(id);
}

// static
void PipewireRemoteAudioInput::Core::OnRegistryGlobalRemove(void* data,
                                                            uint32_t id) {
  static_cast<Core*>(data)->HandleRegistryGlobalRemove(id);
}

void PipewireRemoteAudioInput::Core::HandleRegistryGlobalRemove(uint32_t id) {
  pending_links_.erase(id);
  if (active_links_.erase(id) > 0 && active_links_.empty()) {
    on_active_consumers_changed_cb_.Run(false);
  }
}

PipewireRemoteAudioInput::PipewireRemoteAudioInput() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PipewireRemoteAudioInput::~PipewireRemoteAudioInput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool PipewireRemoteAudioInput::Start(base::WeakPtr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  core_ = std::make_unique<Core>();
  if (!core_->Start(delegate)) {
    core_.reset();
    return false;
  }
  return true;
}

void PipewireRemoteAudioInput::OnAudioPacket(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_) << "Start() has not been called.";

  core_->OnAudioPacket(std::move(packet));
}

}  // namespace remoting
