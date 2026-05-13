// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_injector.h"

#include "build/build_config.h"
#include "remoting/base/fifo_buffer.h"

#if BUILDFLAG(IS_LINUX)
#include "remoting/host/linux/pipewire_audio_injector.h"
#endif

namespace remoting {

AudioInjector::AudioInjector() = default;

AudioInjector::~AudioInjector() = default;

// static
bool AudioInjector::IsSupported() {
#if BUILDFLAG(IS_LINUX)
  // On Linux, we check if PipeWire is available and can be initialized.
  // Note that in multi-process mode, this may return true in the network
  // process because the libraries are loadable, even though the virtual audio
  // device can only be created and used in the desktop process due to
  // PipeWire's user isolation.
  return PipewireAudioInjector::IsSupported();
#else
  return false;
#endif
}

// static
std::unique_ptr<AudioInjector> AudioInjector::Create(
    std::unique_ptr<FifoBufferReader> audio_reader) {
#if BUILDFLAG(IS_LINUX)
  return PipewireAudioInjector::Create(std::move(audio_reader));
#else
  return nullptr;
#endif
}

}  // namespace remoting
