// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_TEST_WEB_THREAD_H_
#define IOS_WEB_PUBLIC_TEST_TEST_WEB_THREAD_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "ios/web/public/thread/web_thread.h"

namespace base {
class MessageLoop;
class Thread;
}

namespace web {

class WebSubThread;
class WebThreadImpl;

// DEPRECATED: use WebTaskEnvironment instead.
// A WebThread for unit tests; this lets unit tests outside of web create
// WebThread instances.
class TestWebThread {
 public:
  // Constructs a TestWebThread with a `real_thread_` and starts it (with a
  // MessageLoopForIO if `identifier == WebThread::IO`).
  explicit TestWebThread(WebThread::ID identifier);

  // Constructs a TestWebThread "running" on `thread_runner` (no
  // `real_thread_`).
  TestWebThread(WebThread::ID identifier,
                scoped_refptr<base::SingleThreadTaskRunner> thread_runner);

  // Constructs a TestWebThread based on `message_loop` (no `real_thread_`).
  TestWebThread(WebThread::ID identifier, base::MessageLoop* message_loop);

  TestWebThread(const TestWebThread&) = delete;
  TestWebThread& operator=(const TestWebThread&) = delete;

  ~TestWebThread();

  // Provides a subset of the capabilities of the Thread interface to enable
  // certain unit tests. To avoid a stronger dependency of the internals of
  // WebThread, do no provide the full Thread interface.

  // Starts the thread with a generic message loop.
  void Start();

  // Starts the thread with an IOThread message loop.
  void StartIOThread();

  // Together these are the same as StartIOThread(). They can be called in
  // phases to test binding WebThread::IO after its underlying thread was
  // started.
  void StartIOThreadUnregistered();
  void RegisterAsWebThread();

  // Stops the thread.
  void Stop();

 private:
  const WebThread::ID identifier_;

  // A real thread which represents `identifier_` when constructor #1 is used
  // (null otherwise).
  std::unique_ptr<WebSubThread> real_thread_;

  // Binds `identifier_` to `message_loop` when constructor #2 is used (null
  // otherwise).
  std::unique_ptr<WebThreadImpl> fake_thread_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TEST_WEB_THREAD_H_
