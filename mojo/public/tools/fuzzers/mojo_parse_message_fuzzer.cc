// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message.h"
#include "mojo/public/tools/fuzzers/fuzz_impl.h"

void FuzzMessage(const uint8_t* data, size_t size, base::RunLoop* run) {
  mojo::PendingRemote<fuzz::mojom::FuzzInterface> fuzz;
  auto impl = std::make_unique<FuzzImpl>(fuzz.InitWithNewPipeAndPassReceiver());
  auto router = impl->receiver_.internal_state()->RouterForTesting();

  // Create a mojo message with the appropriate payload size.
  mojo::ScopedMessageHandle handle;
  mojo::CreateMessage(&handle, MOJO_CREATE_MESSAGE_FLAG_NONE);
  MojoAppendMessageDataOptions options = {
      .struct_size = sizeof(options),
      .flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE};
  void* buffer;
  uint32_t buffer_size;
  MojoAppendMessageData(handle->value(), static_cast<uint32_t>(size), nullptr,
                        0, &options, &buffer, &buffer_size);
  CHECK_GE(buffer_size, static_cast<uint32_t>(size));
  memcpy(buffer, data, size);

  // Run the message through header validation, payload validation, and
  // dispatch to the impl.
  router->SimulateReceivingMessageForTesting(std::move(handle));

  // Allow the harness function to return now.
  run->Quit();
}

// Environment for the fuzzer. Initializes the mojo EDK and sets up a
// ThreadPool, because Mojo messages must be sent and processed from
// TaskRunners.
struct Environment {
  Environment() : main_thread_task_executor(base::MessagePumpType::UI) {
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
        "MojoParseMessageFuzzerProcess");
    mojo::core::Init();
  }

  // TaskExecutor loop to send and handle messages on.
  base::SingleThreadTaskExecutor main_thread_task_executor;

  // Suppress mojo validation failure logs.
  mojo::internal::ScopedSuppressValidationErrorLoggingForTests log_suppression;
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment* env = new Environment();
  // Pass the data along to run on a TaskExecutor, and wait for it to finish.
  base::RunLoop run;
  env->main_thread_task_executor.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FuzzMessage, data, size, &run));
  run.Run();

  return 0;
}
