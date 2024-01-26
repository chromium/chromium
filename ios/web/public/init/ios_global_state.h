// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_IOS_GLOBAL_STATE_H_
#define IOS_WEB_PUBLIC_INIT_IOS_GLOBAL_STATE_H_

#import "base/memory/raw_ptr.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace base {
class SingleThreadTaskExecutor;
}

namespace ios_global_state {

// Contains parameters passed to `Create`.
struct CreateParams {
  CreateParams() : install_at_exit_manager(false), argc(0), argv(nullptr) {}

  bool install_at_exit_manager;

  int argc;
  raw_ptr<const char*> argv;
};

// Creates global state for iOS. This should be called as early as possible in
// the application lifecycle. It is safe to call this method more than once, the
// initialization will only be performed once.
//
// An AtExitManager will only be created if `register_exit_manager` is true. If
// `register_exit_manager` is false, an AtExitManager must already exist before
// calling `Create`.
// `argc` and `argv` may be set to the command line options which were passed to
// the application.
//
// Since the initialization will only be performed the first time this method is
// called, the values of all the parameters will be ignored after the first
// call.
void Create(const CreateParams& create_params);

// Creates a task executor for the UI thread and attaches it. It is safe to call
// this method more than once, the initialization will only be performed once.
void BuildSingleThreadTaskExecutor();

// Destroys the message loop create by BuildSingleThreadTaskExecutor. It is safe
// to call multiple times.
void DestroySingleThreadTaskExecutor();

// Creates a network change notifier.  It is safe to call this method more than
// once, the initialization will only be performed once.
void CreateNetworkChangeNotifier();

// Destroys the network change notifier created by CreateNetworkChangeNotifier.
// It is safe to call this method multiple time.
void DestroyNetworkChangeNotifier();

// Starts a global base::ThreadPoolInstance. This method must be called to start
// the Thread Pool that is created in `Create`. It is safe to call this method
// more than once, the thread pool will only be started once.
void StartThreadPool();

// Destroys the AtExitManager if one was created in `Create`. It is safe to call
// this method even if `install_at_exit_manager` was false in the CreateParams
// passed to `Create`. It is safe to call this method more than once, the
// AtExitManager will be destroyed on the first call.
void DestroyAtExitManager();

// Returns SingleThreadTaskExecutor for the UI thread.
base::SingleThreadTaskExecutor* GetMainThreadTaskExecutor();

}  // namespace ios_global_state

#endif  // IOS_WEB_PUBLIC_INIT_IOS_GLOBAL_STATE_H_
