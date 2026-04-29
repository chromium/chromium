// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_utils.h"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "remoting/base/logging.h"

namespace remoting {

DISABLE_CFI_DLSYM
void PipewireThreadLoopDeleter::operator()(struct pw_thread_loop* loop) {
  if (loop) {
    GetPipewireLoader().pw_thread_loop_destroy(loop);
  }
}

DISABLE_CFI_DLSYM
void PipewireContextDeleter::operator()(struct pw_context* context) {
  if (context) {
    GetPipewireLoader().pw_context_destroy(context);
  }
}

DISABLE_CFI_DLSYM
void PipewireCoreDeleter::operator()(struct pw_core* core) {
  if (core) {
    GetPipewireLoader().pw_core_disconnect(core);
  }
}

DISABLE_CFI_DLSYM
void PipewireStreamDeleter::operator()(struct pw_stream* stream) {
  if (stream) {
    GetPipewireLoader().pw_stream_destroy(stream);
  }
}

DISABLE_CFI_DLSYM
void PipewireProxyDeleter::operator()(struct pw_proxy* proxy) {
  if (proxy) {
    GetPipewireLoader().pw_proxy_destroy(proxy);
  }
}

DISABLE_CFI_DLSYM
ScopedThreadLoopLock::ScopedThreadLoopLock(struct pw_thread_loop* loop)
    : loop_(loop) {
  if (loop_) {
    GetPipewireLoader().pw_thread_loop_lock(loop_);
  }
}

DISABLE_CFI_DLSYM
ScopedThreadLoopLock::~ScopedThreadLoopLock() {
  if (loop_) {
    GetPipewireLoader().pw_thread_loop_unlock(loop_);
  }
}

RemotingPipewireLoader& GetPipewireLoader() {
  static base::NoDestructor<RemotingPipewireLoader> pipewire_loader;
  return *pipewire_loader;
}

DISABLE_CFI_DLSYM
bool EnsurePipewireInitialized() {
  RemotingPipewireLoader& loader = GetPipewireLoader();
  if (loader.loaded()) {
    return true;
  }

  // Try to load the library with the default name. This will succeed if
  // PipeWire is installed on the system and the library is in the standard
  // search path.
  if (loader.Load("libpipewire-0.3.so.0")) {
    loader.pw_init(nullptr, nullptr);
    return true;
  }

  HOST_LOG << "Cannot load PipeWire library.";
  return false;
}

DISABLE_CFI_DLSYM
ScopedPipewireStream CreatePipewireStream(
    ScopedPipewireCore& core,
    struct pw_properties* props,
    const struct pw_stream_events* stream_events,
    void* data,
    struct spa_hook* listener,
    pw_direction direction,
    uint32_t rate,
    uint32_t channels) {
  ScopedPipewireStream stream(GetPipewireLoader().pw_stream_new(
      core.get(), spa_dict_lookup(&props->dict, PW_KEY_NODE_NAME), props));
  if (!stream) {
    LOG(ERROR) << "Failed to create PipeWire stream.";
    return ScopedPipewireStream();
  }

  GetPipewireLoader().pw_stream_add_listener(stream.get(), listener,
                                             stream_events, data);

  uint8_t buffer[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const struct spa_pod* params[1];

  struct spa_audio_info_raw info =
      SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16_LE, .rate = rate,
                              .channels = channels);
  if (channels == 1) {
    info.position[0] = SPA_AUDIO_CHANNEL_MONO;
  } else if (channels == 2) {
    info.position[0] = SPA_AUDIO_CHANNEL_FL;
    info.position[1] = SPA_AUDIO_CHANNEL_FR;
  } else {
    LOG(ERROR) << "Unsupported number of channels: " << channels;
    return ScopedPipewireStream();
  }

  params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

  if (GetPipewireLoader().pw_stream_connect(
          stream.get(), direction, PW_ID_ANY,
          static_cast<pw_stream_flags>(PW_STREAM_FLAG_MAP_BUFFERS |
                                       PW_STREAM_FLAG_RT_PROCESS),
          params, 1) < 0) {
    LOG(ERROR) << "Failed to connect PipeWire stream.";
    return ScopedPipewireStream();
  }

  return stream;
}

}  // namespace remoting
