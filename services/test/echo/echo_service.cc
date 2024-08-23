// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/echo/echo_service.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/immediate_crash.h"
#include "base/memory/shared_memory_mapping.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <winevt.h>

#include "base/native_library.h"
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
  base::span(mapping).copy_prefix_from(base::as_byte_span(input));
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

void EchoService::LoadNativeLibrary(const ::base::FilePath& library,
                                    bool call_sec32_delayload,
                                    LoadNativeLibraryCallback callback) {
  // This attempts to load a library inside the sandbox - it should fail unless
  // the library was in `ServiceProcessHostOptions::WithPreloadedLibraries()`.
  base::NativeLibraryLoadError error;
  // We leak the module as preloading already leaked it.
  HMODULE hmod = base::LoadNativeLibrary(library, &error);
  if (!hmod) {
    std::move(callback).Run(LoadStatus::kFailedLoadLibrary, error.code);
    return;
  }

  // Calls an exported function that calls a delayloaded function that should
  // be loaded in the utility (as secur32.dll is imported by chrome.dll).
  if (call_sec32_delayload) {
    BOOL(WINAPI * fn)() = nullptr;
    fn = reinterpret_cast<decltype(fn)>(
        GetProcAddress(hmod, "FnCallsDelayloadFn"));
    if (!fn) {
      std::move(callback).Run(LoadStatus::kFailedGetProcAddress,
                              GetLastError());
      return;
    }
    BOOL ret = fn();
    if (!ret) {
      std::move(callback).Run(LoadStatus::kFailedCallingDelayLoad,
                              GetLastError());
      return;
    }
  }
  std::move(callback).Run(LoadStatus::kSuccess, ERROR_SUCCESS);
}
#endif  // BUILDFLAG(IS_WIN)

void EchoService::DecryptEncrypt(os_crypt_async::Encryptor encryptor,
                                 const std::vector<uint8_t>& input,
                                 DecryptEncryptCallback callback) {
  // OSCrypt sync services are not available because they are not initialized in
  // a child process.
  CHECK(!OSCrypt::IsEncryptionAvailable());

  CHECK(encryptor.IsDecryptionAvailable());
  // Take the input, which was encrypted in the caller process, and decrypt it.
  const auto plaintext = encryptor.DecryptData(input);
  if (!plaintext.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  CHECK(encryptor.IsEncryptionAvailable());
  // Encrypt it again using the key inside this process, and return the
  // encrypted ciphertext to the caller.
  std::move(callback).Run(encryptor.EncryptString(*plaintext));
}

}  // namespace echo
