// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/enclave/fido_enclave_device.h"

namespace device {
namespace {

const GURL kLocalUrl = GURL("http://127.0.0.1:8880");

// Corresponds to identity seed {1, 2, 3, 4}.
const uint8_t kPeerPublicKey[] = {
    4,   244, 60,  222, 80,  52,  238, 134, 185, 2,   84,  48,  248,
    87,  211, 219, 145, 204, 130, 45,  180, 44,  134, 205, 239, 90,
    127, 34,  229, 225, 93,  163, 51,  206, 28,  47,  134, 238, 116,
    86,  252, 239, 210, 98,  147, 46,  198, 87,  75,  254, 37,  114,
    179, 110, 145, 23,  34,  208, 25,  171, 184, 129, 14,  84,  80};

// This is an executable test harness that wraps FidoEnclaveDevice and can
// initiate transactions.
// TODO(kenrb): Delete class and file this when FidoEnclaveDevice is properly
// integrated as a FIDO device and has proper unit tests.
class EnclaveTestClient {
 public:
  EnclaveTestClient() = default;

  int StartTransaction();

 private:
  void Terminate(absl::optional<std::vector<uint8_t>> result);

  std::unique_ptr<FidoEnclaveDevice> device_;

  base::RunLoop run_loop_;
};

int EnclaveTestClient::StartTransaction() {
  device_ = std::make_unique<FidoEnclaveDevice>(kLocalUrl, kPeerPublicKey);
  std::vector<uint8_t> msg = {'a', 'b', 'c', 'd'};
  device_->DeviceTransact(msg, base::BindOnce(&EnclaveTestClient::Terminate,
                                              base::Unretained(this)));

  run_loop_.Run();
  return 0;
}

void EnclaveTestClient::Terminate(absl::optional<std::vector<uint8_t>> result) {
  if (result) {
    std::cout << "Result from transaction: "
              << base::HexEncode(result->data(), result->size()) << "\n";
  } else {
    std::cout << "No result received";
  }

  run_loop_.Quit();
}

}  // namespace
}  // namespace device

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("passkey_enclave");
  base::ScopedClosureRunner cleanup(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  device::EnclaveTestClient client;
  return client.StartTransaction();
}
