// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_H_
#define CONTENT_CHILD_CHILD_PROCESS_H_

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {
class ChildThreadImpl;

// Base class for child processes of the browser process (i.e. renderer and
// plugin host). This is a singleton object for each child process.
//
// The constructor will call ThreadPoolInstance::Start() unless a ThreadPool is
// already running, which can happen when the ChildProcess object is
// instantiated in the browser process or in tests.
//
// During process shutdown the following sequence of actions happens in
// order.
//
// 1. ChildProcess::~ChildProcess() is called.
//   2. Shutdown event is fired. Background threads should stop.
//   3. ChildThreadImpl::Shutdown() is called. ChildThread is also deleted.
//   4. IO thread is stopped.
//   5. ThreadPoolInstance::Shutdown() is called if the constructor called
//      ThreadPoolInstance::Start().
// 6. Main message loop exits.
// 7. Child process is now fully stopped.
//
// Note: IO thread outlives the ChildThreadImpl object.
class CONTENT_EXPORT ChildProcess {
 public:
  // Child processes should have an object that derives from this class.
  // Normally you would immediately call set_main_thread after construction.
  // |io_thread_type| is the type of the IO thread.
  // |thread_pool_init_params| is used to start the ThreadPool. Default params
  // are used if |thread_pool_init_params| is nullptr. It is ignored if a
  // ThreadPool is already running.
  explicit ChildProcess(
      base::ThreadType io_thread_type = base::ThreadType::kDefault,
      std::unique_ptr<base::ThreadPoolInstance::InitParams>
          thread_pool_init_params = nullptr);

  // This constructor can be used to create a ChildProcess within the browser
  // process which shares the IO thread. `io_thread_runner` passes an existing
  // task runner to use for the child IO thread instead of creating a new
  // thread.
  explicit ChildProcess(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner);

  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;

  virtual ~ChildProcess();

  // May be NULL if the main thread hasn't been set explicitly.
  ChildThreadImpl* main_thread();

  // Sets the object associated with the main thread of this process.
  // Takes ownership of the pointer.
  void set_main_thread(ChildThreadImpl* thread);

  // We need to stop the IO thread here instead of just flushing it, so that it
  // can no longer post tasks back to the main thread.
  void StopIOThreadForTesting() { io_thread_->Stop(); }

  base::SingleThreadTaskRunner* io_task_runner() {
    return io_thread_runner_.get();
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Changes the thread type of the child process IO thread.
  void SetIOThreadType(base::ThreadType thread_type);
#endif

  // A global event object that is signalled when the main thread's message
  // loop exits.  This gives background threads a way to observe the main
  // thread shutting down.  This can be useful when a background thread is
  // waiting for some information from the browser process.  If the browser
  // process goes away prematurely, the background thread can at least notice
  // the child processes's main thread exiting to determine that it should give
  // up waiting.
  // For example, see the renderer code used to implement GetCookies().
  base::WaitableEvent* GetShutDownEvent();

  // These are used for ref-counting the child process.  The process shuts
  // itself down when the ref count reaches 0.
  //
  // This is not used for renderer processes. The browser process is the only
  // one responsible for shutting them down. See mojo::KeepAliveHandle and more
  // generally the RenderProcessHostImpl class if you want to keep the renderer
  // process alive longer.
  virtual void AddRefProcess();
  virtual void ReleaseProcess();

  // Getter for the one ChildProcess object for this process. Can only be called
  // on the main thread.
  static ChildProcess* current();

 private:
  const base::AutoReset<ChildProcess*> resetter_;

  int ref_count_ = 0;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  // The thread that handles IO events. May be null if `io_thread_runner` was
  // passed to the constructor.
  std::unique_ptr<base::Thread> io_thread_;

  // The task runner to use for IO thread tasks.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner_;

  // NOTE: make sure that main_thread_ is listed after shutdown_event_, since
  // it depends on it (indirectly through IPC::SyncChannel).  Same for
  // io_thread_.
  std::unique_ptr<ChildThreadImpl> main_thread_;

  // Whether this ChildProcess initialized ThreadPoolInstance.
  bool initialized_thread_pool_ = false;
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_H_
