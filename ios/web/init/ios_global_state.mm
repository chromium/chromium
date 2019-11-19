// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/init/ios_global_state.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/initialization_util.h"
#include "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

base::AtExitManager* g_exit_manager = nullptr;
base::SingleThreadTaskExecutor* g_task_executor = nullptr;
net::NetworkChangeNotifier* g_network_change_notifer = nullptr;

}  // namespace

namespace ios_global_state {

void Create(const CreateParams& create_params) {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    if (create_params.install_at_exit_manager) {
      g_exit_manager = new base::AtExitManager();
    }
    base::CommandLine::Init(create_params.argc, create_params.argv);

    base::ThreadPoolInstance::Create("Browser");
  });
}

void BuildSingleThreadTaskExecutor() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    // Create a SingleThreadTaskExecutor if one does not already exist for the
    // current thread.
    if (!base::MessageLoopCurrent::Get()) {
      g_task_executor =
          new base::SingleThreadTaskExecutor(base::MessagePumpType::UI);
    }
  });
}

void DestroySingleThreadTaskExecutor() {
  delete g_task_executor;
  g_task_executor = nullptr;
}

void CreateNetworkChangeNotifier() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    g_network_change_notifer =
        net::NetworkChangeNotifier::CreateIfNeeded().release();
  });
}

void DestroyNetworkChangeNotifier() {
  delete g_network_change_notifer;
  g_network_change_notifer = nullptr;
}

void StartThreadPool() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    constexpr int kMinForegroundThreads = 6;
    constexpr int kMaxForegroundThreads = 16;
    constexpr double kCoreMultiplierForegroundThreads = 0.6;
    constexpr int kOffsetForegroundThreads = 0;
    base::ThreadPoolInstance::Get()->Start(
        {base::RecommendedMaxNumberOfThreadsInThreadGroup(
            kMinForegroundThreads, kMaxForegroundThreads,
            kCoreMultiplierForegroundThreads, kOffsetForegroundThreads)});
  });
}

void DestroyAtExitManager() {
  delete g_exit_manager;
  g_exit_manager = nullptr;
}

base::SingleThreadTaskExecutor* GetMainThreadTaskExecutor() {
  return g_task_executor;
}

}  // namespace ios_global_state
