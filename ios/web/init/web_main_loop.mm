// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/init/web_main_loop.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/process_metrics.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#import "ios/web/net/cookie_notification_bridge.h"
#include "ios/web/public/init/ios_global_state.h"
#include "ios/web/public/init/web_main_parts.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/web_client.h"
#include "ios/web/web_sub_thread.h"
#include "ios/web/web_thread_impl.h"
#include "ios/web/webui/url_data_manager_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// The currently-running WebMainLoop.  There can be one or zero.
// TODO(crbug.com/965889): Desktop uses this to implement
// ImmediateShutdownAndExitProcess.  If we don't need that functionality, we can
// remove this.
WebMainLoop* g_current_web_main_loop = nullptr;

WebMainLoop::WebMainLoop()
    : result_code_(0),
      created_threads_(false),
      destroy_task_executor_(
          base::BindOnce(&ios_global_state::DestroySingleThreadTaskExecutor)),
      destroy_network_change_notifier_(
          base::BindOnce(&ios_global_state::DestroyNetworkChangeNotifier)) {
  DCHECK(!g_current_web_main_loop);
  g_current_web_main_loop = this;
}

WebMainLoop::~WebMainLoop() {
  DCHECK_EQ(this, g_current_web_main_loop);
  g_current_web_main_loop = nullptr;
}

void WebMainLoop::Init() {
  parts_ = web::GetWebClient()->CreateWebMainParts();
}

void WebMainLoop::EarlyInitialization() {
  if (parts_) {
    parts_->PreEarlyInitialization();
    parts_->PostEarlyInitialization();
  }
}

void WebMainLoop::MainMessageLoopStart() {
  if (parts_) {
    parts_->PreMainMessageLoopStart();
  }

  ios_global_state::BuildSingleThreadTaskExecutor();

  InitializeMainThread();

  // TODO(crbug.com/807279): Do we need PowerMonitor on iOS, or can we get rid
  // of it?
  base::PowerMonitor::Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());

  ios_global_state::CreateNetworkChangeNotifier();

  if (parts_) {
    parts_->PostMainMessageLoopStart();
  }
}

void WebMainLoop::CreateStartupTasks() {
  int result = 0;
  result = PreCreateThreads();
  if (result > 0)
    return;

  result = CreateThreads();
  if (result > 0)
    return;

  result = WebThreadsStarted();
  if (result > 0)
    return;

  result = PreMainMessageLoopRun();
  if (result > 0)
    return;
}

int WebMainLoop::PreCreateThreads() {
  if (parts_) {
    parts_->PreCreateThreads();
  }

  return result_code_;
}

int WebMainLoop::CreateThreads() {
  ios_global_state::StartThreadPool();

  base::Thread::Options io_message_loop_options;
  io_message_loop_options.message_pump_type = base::MessagePumpType::IO;
  io_thread_ = std::make_unique<WebSubThread>(WebThread::IO);
  if (!io_thread_->StartWithOptions(io_message_loop_options))
    LOG(FATAL) << "Failed to start WebThread::IO";
  io_thread_->RegisterAsWebThread();

  // Only start IO thread above as this is the only WebThread besides UI (which
  // is the main thread).
  static_assert(WebThread::ID_COUNT == 2, "Unhandled WebThread");

  created_threads_ = true;
  return result_code_;
}

int WebMainLoop::PreMainMessageLoopRun() {
  if (parts_) {
    parts_->PreMainMessageLoopRun();
  }

  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow unresponsive tasks from the UI thread.
  base::DisallowUnresponsiveTasks();
  return result_code_;
}

void WebMainLoop::ShutdownThreadsAndCleanUp() {
  if (!created_threads_) {
    // Called early, nothing to do
    return;
  }

  // Teardown may start in PostMainMessageLoopRun, and during teardown we
  // need to be able to perform IO.
  base::ThreadRestrictions::SetIOAllowed(true);
  base::PostTask(
      FROM_HERE, {WebThread::IO},
      base::BindOnce(
          base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed), true));

  // Also allow waiting to join threads.
  // TODO(crbug.com/800808): Ideally this (and the above SetIOAllowed()
  // would be scoped allowances). That would be one of the first step to ensure
  // no persistent work is being done after ThreadPoolInstance::Shutdown() in
  // order to move towards atomic shutdown.
  base::ThreadRestrictions::SetWaitAllowed(true);
  base::PostTask(
      FROM_HERE, {WebThread::IO},
      base::BindOnce(
          base::IgnoreResult(&base::ThreadRestrictions::SetWaitAllowed), true));

  if (parts_) {
    parts_->PostMainMessageLoopRun();
  }

  io_thread_.reset();

  // Only stop IO thread above as this is the only WebThread besides UI (which
  // is the main thread).
  static_assert(WebThread::ID_COUNT == 2, "Unhandled WebThread");

  // Shutdown ThreadPool after the other threads. Other threads such as the
  // I/O thread may need to schedule work like closing files or flushing data
  // during shutdown, so ThreadPool needs to be available. There may also be
  // slow operations pending that will block shutdown, so closing it here (which
  // will block until required operations are complete) gives more head start
  // for those operations to finish.
  base::ThreadPoolInstance::Get()->Shutdown();

  URLDataManagerIOS::DeleteDataSources();

  if (parts_) {
    parts_->PostDestroyThreads();
  }
}

void WebMainLoop::InitializeMainThread() {
  base::PlatformThread::SetName("CrWebMain");

  // Register the main thread by instantiating it, but don't call any methods.
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  main_thread_.reset(new WebThreadImpl(
      WebThread::UI,
      ios_global_state::GetMainThreadTaskExecutor()->task_runner()));
}

int WebMainLoop::WebThreadsStarted() {
  cookie_notification_bridge_.reset(new CookieNotificationBridge);
  return result_code_;
}

}  // namespace web
