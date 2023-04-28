// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/echo/echo_service.h"

#include "base/immediate_crash.h"
#include "base/memory/shared_memory_mapping.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include <winevt.h>
#endif

#include <string>

namespace echo {

EchoService::EchoService(mojo::PendingReceiver<mojom::EchoService> receiver)
    : receiver_(this, std::move(receiver)) {}

EchoService::~EchoService() = default;

void EchoService::EchoString(const std::string& input,
                             EchoStringCallback callback) {
  std::move(callback).Run(input);
}

void EchoService::EchoStringToSharedMemory(
    const std::string& input,
    base::UnsafeSharedMemoryRegion region) {
  base::WritableSharedMemoryMapping mapping = region.Map();
  memcpy(mapping.memory(), input.data(), input.size());
}

void EchoService::Quit() {
  receiver_.reset();
}

void EchoService::Crash() {
  base::ImmediateCrash();
}

#if BUILDFLAG(IS_WIN)
void EchoService::DelayLoad() {
  // This causes wevtapi.dll to be delay loaded. It should not work from inside
  // a sandboxed process.
  EVT_HANDLE handle = ::EvtCreateRenderContext(0, nullptr, 0);
  ::EvtClose(handle);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace echo
