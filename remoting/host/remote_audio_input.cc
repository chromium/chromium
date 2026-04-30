// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_audio_input.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include "remoting/host/linux/pipewire_remote_audio_input.h"
#endif

namespace remoting {

// static
bool RemoteAudioInput::IsSupported() {
#if BUILDFLAG(IS_LINUX)
  // On Linux, we check if PipeWire is available and can be initialized.
  // Note that in multi-process mode, this may return true in the network
  // process because the libraries are loadable, even though the virtual audio
  // device can only be created and used in the desktop process due to
  // PipeWire's user isolation.
  return PipewireRemoteAudioInput::IsSupported();
#else
  return false;
#endif
}

}  // namespace remoting
