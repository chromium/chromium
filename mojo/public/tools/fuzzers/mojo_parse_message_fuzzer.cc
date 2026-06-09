// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message.h"
#include "mojo/public/tools/fuzzers/fuzz_impl.h"
#include "mojo/public/tools/fuzzers/suppress_validation_error_logging.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/test_support_android.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/at_exit.h"
#endif  // BUILDFLAG(IS_WIN)

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
  UNSAFE_TODO(memcpy(buffer, data, size));

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
  Environment() {
#if BUILDFLAG(IS_ANDROID)
    // On Android, SingleThreadTaskExecutor with MessagePumpType::UI creates a
    // UI message pump that does not support RunLoop::Run(). This installs a
    // stub pump to allow it in tests.
    base::InitAndroidTestMessageLoop();
#endif
    main_thread_task_executor =
        std::make_unique<base::SingleThreadTaskExecutor>(
            base::MessagePumpType::UI);
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
        "MojoParseMessageFuzzerProcess");
    mojo::core::Init();
  }

#if BUILDFLAG(IS_WIN)
  // Windows thread executor has a dependency on AtExitManager.
  std::unique_ptr<base::AtExitManager> at_exit_manager_ =
      std::make_unique<base::AtExitManager>();
#endif  // BUILDFLAG(IS_WIN)

  // TaskExecutor loop to send and handle messages on.
  std::unique_ptr<base::SingleThreadTaskExecutor> main_thread_task_executor;

  // Suppress mojo validation failure logs.
  mojo::internal::ScopedSuppressValidationErrorLoggingForTests log_suppression;
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment* env = new Environment();
  // Pass the data along to run on a TaskExecutor, and wait for it to finish.
  base::RunLoop run;
  env->main_thread_task_executor->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FuzzMessage, data, size, &run));
  run.Run();

  return 0;
}
