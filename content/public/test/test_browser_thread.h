// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class Thread;
}

namespace content {

class BrowserProcessSubThread;
class BrowserThreadImpl;

// DEPRECATED: use TestBrowserThreadBundle instead. See http://crbug.com/272091
// A BrowserThread for unit tests; this lets unit tests in chrome/ create
// BrowserThread instances.
class TestBrowserThread {
 public:
  // Constructs a TestBrowserThread with a |real_thread_| and starts it (with a
  // MessageLoopForIO if |identifier == BrowserThread::IO|.
  explicit TestBrowserThread(BrowserThread::ID identifier);

  // Constructs a TestBrowserThread "running" on |thread_runner| (no
  // |real_thread_|).
  TestBrowserThread(BrowserThread::ID identifier,
                    scoped_refptr<base::SingleThreadTaskRunner> thread_runner);

  ~TestBrowserThread();

  // We provide a subset of the capabilities of the Thread interface
  // to enable certain unit tests.  To avoid a stronger dependency of
  // the internals of BrowserThread, we do not provide the full Thread
  // interface.

  // Starts the thread with a generic message loop.
  void Start();

  // Starts the thread with a generic message loop and waits for the
  // thread to run.
  void StartAndWaitForTesting();

  // Starts the thread with an IOThread message loop.
  void StartIOThread();

  // Together these are the same as StartIOThread(). They can be called in
  // phases to test binding BrowserThread::IO after its underlying thread was
  // started.
  void StartIOThreadUnregistered();
  void RegisterAsBrowserThread();

  // Stops the thread, no-op if this is not a real thread.
  void Stop();

 private:
  const BrowserThread::ID identifier_;

  // A real thread which represents |identifier_| when constructor #1 is used
  // (null otherwise).
  std::unique_ptr<BrowserProcessSubThread> real_thread_;

  // Binds |identifier_| to |message_loop| when constructor #2 is used (null
  // otherwise).
  std::unique_ptr<BrowserThreadImpl> fake_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserThread);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_BROWSER_THREAD_H_
