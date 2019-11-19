// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gin/array_buffer.h"
#include "gin/modules/console.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "gin/try_catch.h"
#include "gin/v8_initializer.h"

namespace gin {
namespace {

std::string Load(const base::FilePath& path) {
  std::string source;
  if (!ReadFileToString(path, &source))
    LOG(FATAL) << "Unable to read " << path.LossyDisplayName();
  return source;
}

void Run(base::WeakPtr<Runner> runner, const base::FilePath& path) {
  if (!runner)
    return;
  Runner::Scope scope(runner.get());
  runner->Run(Load(path), path.AsUTF8Unsafe());
}

class GinShellRunnerDelegate : public ShellRunnerDelegate {
 public:
  GinShellRunnerDelegate() {}

  v8::Local<v8::ObjectTemplate> GetGlobalTemplate(
      ShellRunner* runner,
      v8::Isolate* isolate) override {
    v8::Local<v8::ObjectTemplate> templ =
        ObjectTemplateBuilder(isolate).Build();
    gin::Console::Register(isolate, templ);
    return templ;
  }

  void UnhandledException(ShellRunner* runner, TryCatch& try_catch) override {
    LOG(ERROR) << try_catch.GetStackTrace();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GinShellRunnerDelegate);
};

}  // namespace
}  // namespace gin

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  base::i18n::InitializeICU();
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif

  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("gin");

  // Initialize the base::FeatureList since IsolateHolder can depend on it.
  base::FeatureList::SetInstance(base::WrapUnique(new base::FeatureList));

  {
    gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                   gin::ArrayBufferAllocator::SharedInstance());
    gin::IsolateHolder instance(
        base::ThreadTaskRunnerHandle::Get(),
        gin::IsolateHolder::IsolateType::kBlinkMainThread);

    gin::GinShellRunnerDelegate delegate;
    gin::ShellRunner runner(&delegate, instance.isolate());

    {
      gin::Runner::Scope scope(&runner);
      runner.GetContextHolder()
          ->isolate()
          ->SetCaptureStackTraceForUncaughtExceptions(true);
    }

    base::CommandLine::StringVector args =
        base::CommandLine::ForCurrentProcess()->GetArgs();
    for (base::CommandLine::StringVector::const_iterator it = args.begin();
         it != args.end(); ++it) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(gin::Run, runner.GetWeakPtr(), base::FilePath(*it)));
    }

    base::RunLoop().RunUntilIdle();
  }

  // gin::IsolateHolder waits for tasks running in ThreadPool in its
  // destructor and thus must be destroyed before ThreadPool starts skipping
  // CONTINUE_ON_SHUTDOWN tasks.
  base::ThreadPoolInstance::Get()->Shutdown();

  return 0;
}
