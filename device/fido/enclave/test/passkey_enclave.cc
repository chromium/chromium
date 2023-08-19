// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <iostream>
#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/enclave/test/enclave_http_server.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace device {
namespace {

const std::array<uint8_t, cablev2::kQRSeedSize> kIdentitySeed = {1, 2, 3, 4};

class EnclaveTestService {
 public:
  EnclaveTestService() = default;
  ~EnclaveTestService() = default;

  EnclaveTestService(const EnclaveTestService&) = delete;
  EnclaveTestService& operator=(const EnclaveTestService&) = delete;

  int Start();

 private:
  base::RunLoop run_loop_;
  std::unique_ptr<enclave::EnclaveHttpServer> enclave_server_;
};

int EnclaveTestService::Start() {
  enclave_server_ = std::make_unique<enclave::EnclaveHttpServer>(
      kIdentitySeed, run_loop_.QuitClosure());
  enclave_server_->StartServer();

  run_loop_.Run();
  return 0;
}

}  // namespace
}  // namespace device

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(0, nullptr);
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("passkey_enclave");
  auto cleanup = absl::MakeCleanup([] {
    base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); });
  });
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  device::EnclaveTestService service;

  // Returns when service shuts down.
  return service.Start();
}
