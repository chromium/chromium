// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auto_thread.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/base/dynamic_annotations.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace remoting {

namespace {

#if BUILDFLAG(IS_WIN)
std::unique_ptr<base::win::ScopedCOMInitializer> CreateComInitializer(
    AutoThread::ComInitType type) {
  std::unique_ptr<base::win::ScopedCOMInitializer> initializer;
  if (type == AutoThread::COM_INIT_MTA) {
    initializer = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
  } else if (type == AutoThread::COM_INIT_STA) {
    initializer = std::make_unique<base::win::ScopedCOMInitializer>();
  }
  return initializer;
}
#endif

}  // namespace

// Used to pass data to ThreadMain.  This structure is allocated on the stack
// from within StartWithType.
struct AutoThread::StartupData {
  // Fields describing the desired thread behaviour.
  base::MessagePumpType pump_type;

  // Used to receive the AutoThreadTaskRunner for the thread.
  scoped_refptr<AutoThreadTaskRunner> task_runner;

  // Used to synchronize thread startup.
  base::WaitableEvent event;

  explicit StartupData(base::MessagePumpType type)
      : pump_type(type),
        event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}
};

// static
scoped_refptr<AutoThreadTaskRunner> AutoThread::CreateWithType(
    const char* name,
    scoped_refptr<AutoThreadTaskRunner> joiner,
    base::MessagePumpType type) {
  AutoThread* thread = new AutoThread(name, joiner.get());
  scoped_refptr<AutoThreadTaskRunner> task_runner = thread->StartWithType(type);
  if (!task_runner.get()) {
    delete thread;
  }
  return task_runner;
}

// static
scoped_refptr<AutoThreadTaskRunner> AutoThread::Create(
    const char* name,
    scoped_refptr<AutoThreadTaskRunner> joiner) {
  return CreateWithType(name, joiner, base::MessagePumpType::DEFAULT);
}

#if BUILDFLAG(IS_WIN)
// static
scoped_refptr<AutoThreadTaskRunner> AutoThread::CreateWithLoopAndComInitTypes(
    const char* name,
    scoped_refptr<AutoThreadTaskRunner> joiner,
    base::MessagePumpType pump_type,
    ComInitType com_init_type) {
  AutoThread* thread = new AutoThread(name, joiner.get());
  thread->SetComInitType(com_init_type);
  scoped_refptr<AutoThreadTaskRunner> task_runner =
      thread->StartWithType(pump_type);
  if (!task_runner.get()) {
    delete thread;
  }
  return task_runner;
}
#endif

AutoThread::AutoThread(const char* name)
    : startup_data_(nullptr),
#if BUILDFLAG(IS_WIN)
      com_init_type_(COM_INIT_NONE),
#endif
      thread_(),
      name_(name),
      was_quit_properly_(false) {
  thread_checker_.DetachFromThread();
}

AutoThread::AutoThread(const char* name, AutoThreadTaskRunner* joiner)
    : startup_data_(nullptr),
#if BUILDFLAG(IS_WIN)
      com_init_type_(COM_INIT_NONE),
#endif
      thread_(),
      name_(name),
      was_quit_properly_(false),
      joiner_(joiner) {
  thread_checker_.DetachFromThread();
}

AutoThread::~AutoThread() {
  DCHECK(!startup_data_);

  // Wait for the thread to exit.
  if (!thread_.is_null()) {
    // TODO(https://crbug.com/id=944316).
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    base::PlatformThread::Join(thread_);
  }
}

scoped_refptr<AutoThreadTaskRunner> AutoThread::StartWithType(
    base::MessagePumpType type) {
  DCHECK(thread_.is_null());
#if BUILDFLAG(IS_WIN)
  DCHECK(com_init_type_ != COM_INIT_STA || type == base::MessagePumpType::UI);
#endif

  StartupData startup_data(type);
  startup_data_ = &startup_data;

  if (!base::PlatformThread::Create(0, this, &thread_)) {
    DLOG(ERROR) << "failed to create thread";
    startup_data_ = nullptr;
    return nullptr;
  }

  // Wait for the thread to start and initialize single_thread_task_executor
  // TODO(wez): Since at this point we know the MessageLoop _will_ run, and
  // the thread lifetime is controlled by the AutoThreadTaskRunner, we would
  // ideally return the AutoThreadTaskRunner to the caller without waiting for
  // the thread to signal us.
  // TODO(https://crbug.com/id=944316).
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  startup_data.event.Wait();

  // set it to null so we don't keep a pointer to some object on the stack.
  startup_data_ = nullptr;

  DCHECK(startup_data.task_runner.get());
  return startup_data.task_runner;
}

#if BUILDFLAG(IS_WIN)
void AutoThread::SetComInitType(ComInitType com_init_type) {
  DCHECK_EQ(com_init_type_, COM_INIT_NONE);
  com_init_type_ = com_init_type;
}
#endif

void AutoThread::QuitThread(base::OnceClosure quit_when_idle_closure) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::move(quit_when_idle_closure).Run();
  was_quit_properly_ = true;

  if (joiner_.get()) {
    joiner_->PostTask(FROM_HERE,
                      base::BindOnce(&AutoThread::JoinAndDeleteThread,
                                     base::Unretained(this)));
  }
}

void AutoThread::JoinAndDeleteThread() {
  delete this;
}

void AutoThread::ThreadMain() {
  // Bind |thread_checker_| to the current thread.
  DCHECK(thread_checker_.CalledOnValidThread());

  base::SingleThreadTaskExecutor single_thread_task_executor(
      startup_data_->pump_type);
  base::RunLoop run_loop;

  // Complete the initialization of our AutoThread object.
  base::PlatformThread::SetName(name_);
  ABSL_ANNOTATE_THREAD_NAME(name_.c_str());  // Tell the name to race detector.

  // Return an AutoThreadTaskRunner that will cleanly quit this thread when
  // no more references to it remain.
  startup_data_->task_runner = new AutoThreadTaskRunner(
      single_thread_task_executor.task_runner(),
      base::BindOnce(&AutoThread::QuitThread, base::Unretained(this),
                     run_loop.QuitWhenIdleClosure()));

  startup_data_->event.Signal();
  // startup_data_ can't be touched anymore since the starting thread is now
  // unlocked.

#if BUILDFLAG(IS_POSIX)
  // Allow threads running a MessageLoopForIO to use FileDescriptorWatcher.
  std::unique_ptr<base::FileDescriptorWatcher> file_descriptor_watcher;
  if (single_thread_task_executor.type() == base::MessagePumpType::IO) {
    file_descriptor_watcher = std::make_unique<base::FileDescriptorWatcher>(
        single_thread_task_executor.task_runner());
  }
#elif BUILDFLAG(IS_WIN)
  // Initialize COM on the thread, if requested.
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer(
      CreateComInitializer(com_init_type_));
#endif

  run_loop.Run();

  // Assert that MessageLoop::QuitWhenIdle was called by AutoThread::QuitThread.
  DCHECK(was_quit_properly_);
}

}  // namespace remoting
