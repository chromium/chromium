// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TEST_ECHO_ECHO_SERVICE_H_
#define SERVICES_TEST_ECHO_ECHO_SERVICE_H_

#include <vector>

#include "build/build_config.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/test/echo/public/mojom/echo.mojom.h"

namespace echo {

class EchoService : public mojom::EchoService {
 public:
  explicit EchoService(mojo::PendingReceiver<mojom::EchoService> receiver);

  EchoService(const EchoService&) = delete;
  EchoService& operator=(const EchoService&) = delete;

  ~EchoService() override;

 private:
  // mojom::EchoService:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override;
  void EchoStringToSharedMemory(const std::string& input,
                                base::UnsafeSharedMemoryRegion region) override;
  void Quit() override;
  void Crash() override;
#if BUILDFLAG(IS_WIN)
  void DelayLoad() override;
  void LoadNativeLibrary(const ::base::FilePath& library,
                         bool call_sec32_fn,
                         LoadNativeLibraryCallback callback) override;
#endif

  void DecryptEncrypt(os_crypt_async::Encryptor encryptor,
                      const std::vector<uint8_t>& input,
                      DecryptEncryptCallback callback) override;

  mojo::Receiver<mojom::EchoService> receiver_;
};

}  // namespace echo

#endif  // SERVICES_TEST_ECHO_ECHO_SERVICE_H_
