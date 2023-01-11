// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_AUTO_THREAD_H_
#define REMOTING_BASE_AUTO_THREAD_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread_task_runner.h"

namespace remoting {

// Thread implementation that runs a MessageLoop on a new thread, and manages
// the lifetime of the MessageLoop and thread by tracking references to the
// thread's TaskRunner.  The caller passes the thread's TaskRunner to each
// object that needs to run code on the thread, and when no references to the
// TaskRunner remain, the thread will exit.  When the caller destroys this
// object they will be blocked until the thread exits.
// All pending tasks queued on the thread's message loop will run to completion
// before the thread is terminated.
//
// After the thread is stopped, the destruction sequence is:
//
//  (1) Thread::CleanUp()
//  (2) MessageLoop::~MessageLoop
//  (3.b) CurrentThread::DestructionObserver::WillDestroyCurrentMessageLoop
class AutoThread : base::PlatformThread::Delegate {
 public:
  // Create an AutoThread with the specified message-loop |type| and |name|.
  // The supplied AutoThreadTaskRunner will be used to join and delete the
  // new thread when no references to it remain.
  static scoped_refptr<AutoThreadTaskRunner> CreateWithType(
      const char* name,
      scoped_refptr<AutoThreadTaskRunner> joiner,
      base::MessagePumpType type);
  static scoped_refptr<AutoThreadTaskRunner> Create(
      const char* name,
      scoped_refptr<AutoThreadTaskRunner> joiner);

#if BUILDFLAG(IS_WIN)
  // Create an AutoThread initialized for COM.  |com_init_type| specifies the
  // type of COM apartment to initialize.
  enum ComInitType { COM_INIT_NONE, COM_INIT_STA, COM_INIT_MTA };
  static scoped_refptr<AutoThreadTaskRunner> CreateWithLoopAndComInitTypes(
      const char* name,
      scoped_refptr<AutoThreadTaskRunner> joiner,
      base::MessagePumpType pump_type,
      ComInitType com_init_type);
#endif

  // Construct the AutoThread.  |name| identifies the thread for debugging.
  explicit AutoThread(const char* name);

  AutoThread(const AutoThread&) = delete;
  AutoThread& operator=(const AutoThread&) = delete;

  // Waits for the thread to exit, and then destroys it.
  ~AutoThread() override;

  // Starts the thread, running the specified type of MessageLoop.  Returns
  // an AutoThreadTaskRunner through which tasks may be posted to the thread
  // if successful, or NULL on failure.
  //
  // Note: This function can't be called on Windows with the loader lock held;
  // i.e. during a DllMain, global object construction or destruction, atexit()
  // callback.
  //
  // NOTE: You must not call this MessageLoop's Quit method directly.  The
  // thread will exit when no references to the TaskRunner remain.
  scoped_refptr<AutoThreadTaskRunner> StartWithType(base::MessagePumpType type);

#if BUILDFLAG(IS_WIN)
  // Configures the thread to initialize the specified COM apartment type.
  // SetComInitType() must be called before Start().
  void SetComInitType(ComInitType com_init_type);
#endif

 private:
  AutoThread(const char* name, AutoThreadTaskRunner* joiner);

  void QuitThread(base::OnceClosure quit_when_idle_closure);
  void JoinAndDeleteThread();

  // base::PlatformThread::Delegate methods:
  void ThreadMain() override;

  // Used to pass data to ThreadMain.
  struct StartupData;
  raw_ptr<StartupData> startup_data_;

#if BUILDFLAG(IS_WIN)
  // Specifies which kind of COM apartment to initialize, if any.
  ComInitType com_init_type_;
#endif

  // The thread's handle.
  base::PlatformThreadHandle thread_;

  // The name of the thread.  Used for debugging purposes.
  std::string name_;

  // Flag used to indicate whether MessageLoop was quit properly.
  // This allows us to detect premature exit via MessageLoop::QuitWhenIdle().
  bool was_quit_properly_;

  // AutoThreadTaskRunner to post a task to to join & delete this thread.
  scoped_refptr<AutoThreadTaskRunner> joiner_;

  // Verifies that QuitThread() is called on the same thread as ThreadMain().
  base::ThreadChecker thread_checker_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_AUTO_THREAD_H_
