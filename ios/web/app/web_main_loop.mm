// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/app/web_main_loop.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/process_metrics.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/threading/thread_restrictions.h"
#import "ios/web/net/cookie_notification_bridge.h"
#include "ios/web/public/app/web_main_parts.h"
#include "ios/web/public/global_state/ios_global_state.h"
#import "ios/web/public/web_client.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/service_manager_context.h"
#include "ios/web/web_thread_impl.h"
#include "ios/web/webui/url_data_manager_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// The currently-running WebMainLoop.  There can be one or zero.
// TODO(rohitrao): Desktop uses this to implement
// ImmediateShutdownAndExitProcess.  If we don't need that functionality, we can
// remove this.
WebMainLoop* g_current_web_main_loop = nullptr;

WebMainLoop::WebMainLoop()
    : result_code_(0),
      created_threads_(false),
      destroy_message_loop_(base::Bind(&ios_global_state::DestroyMessageLoop)),
      destroy_network_change_notifier_(
          base::Bind(&ios_global_state::DestroyNetworkChangeNotifier)) {
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

  ios_global_state::BuildMessageLoop();

  InitializeMainThread();

  // TODO(crbug.com/807279): Do we need PowerMonitor on iOS, or can we get rid
  // of it?
  std::unique_ptr<base::PowerMonitorSource> power_monitor_source(
      new base::PowerMonitorDeviceSource());
  power_monitor_.reset(new base::PowerMonitor(std::move(power_monitor_source)));

  ios_global_state::CreateNetworkChangeNotifier();

  if (parts_) {
    parts_->PostMainMessageLoopStart();
  }
}

void WebMainLoop::CreateStartupTasks(
    TaskSchedulerInitParamsCallback init_params_callback) {
  int result = 0;
  result = PreCreateThreads();
  if (result > 0)
    return;

  result = CreateThreads(std::move(init_params_callback));
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

int WebMainLoop::CreateThreads(
    TaskSchedulerInitParamsCallback init_params_callback) {
  std::unique_ptr<base::TaskScheduler::InitParams> init_params;
  if (!init_params_callback.is_null()) {
    init_params = std::move(init_params_callback).Run();
  }
  ios_global_state::StartTaskScheduler(init_params.get());

  base::Thread::Options io_message_loop_options;
  io_message_loop_options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_ = std::make_unique<WebThreadImpl>(WebThread::IO);
  io_thread_->StartWithOptions(io_message_loop_options);

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
  base::PostTaskWithTraits(
      FROM_HERE, {WebThread::IO},
      base::Bind(base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed),
                 true));

  if (parts_) {
    parts_->PostMainMessageLoopRun();
  }

  service_manager_context_.reset();

  io_thread_.reset();

  // Only stop IO thread above as this is the only WebThread besides UI (which
  // is the main thread).
  static_assert(WebThread::ID_COUNT == 2, "Unhandled WebThread");

  // Shutdown TaskScheduler after the other threads. Other threads such as the
  // I/O thread may need to schedule work like closing files or flushing data
  // during shutdown, so TaskScheduler needs to be available. There may also be
  // slow operations pending that will block shutdown, so closing it here (which
  // will block until required operations are complete) gives more head start
  // for those operations to finish.
  base::TaskScheduler::GetInstance()->Shutdown();

  URLDataManagerIOS::DeleteDataSources();

  if (parts_) {
    parts_->PostDestroyThreads();
  }
}

void WebMainLoop::InitializeMainThread() {
  base::PlatformThread::SetName("CrWebMain");

  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(
      new WebThreadImpl(WebThread::UI, base::MessageLoop::current()));
}

int WebMainLoop::WebThreadsStarted() {
  cookie_notification_bridge_.reset(new CookieNotificationBridge);
  service_manager_context_ = std::make_unique<ServiceManagerContext>();
  return result_code_;
}

}  // namespace web
