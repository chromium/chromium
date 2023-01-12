// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background_service_manager.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_default.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/simple_thread.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/service_manager.h"

namespace service_manager {

BackgroundServiceManager::BackgroundServiceManager(
    const std::vector<Manifest>& manifests)
    : background_thread_("service_manager") {
  background_thread_.Start();
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundServiceManager::InitializeOnBackgroundThread,
                     base::Unretained(this), manifests));
}

BackgroundServiceManager::~BackgroundServiceManager() {
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundServiceManager::ShutDownOnBackgroundThread,
                     base::Unretained(this), &done_event));
  done_event.Wait();
  DCHECK(!service_manager_);
}

void BackgroundServiceManager::RegisterService(
    const Identity& identity,
    mojo::PendingRemote<mojom::Service> service,
    mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver) {
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BackgroundServiceManager::RegisterServiceOnBackgroundThread,
          base::Unretained(this), identity, std::move(service),
          std::move(metadata_receiver)));
}

void BackgroundServiceManager::InitializeOnBackgroundThread(
    const std::vector<Manifest>& manifests) {
  service_manager_ = std::make_unique<ServiceManager>(
      manifests, ServiceManager::ServiceExecutablePolicy::kSupported);
}

void BackgroundServiceManager::ShutDownOnBackgroundThread(
    base::WaitableEvent* done_event) {
  service_manager_.reset();
  done_event->Signal();
}

void BackgroundServiceManager::RegisterServiceOnBackgroundThread(
    const Identity& identity,
    mojo::PendingRemote<mojom::Service> service,
    mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver) {
  service_manager_->RegisterService(identity, std::move(service),
                                    std::move(metadata_receiver));
}

}  // namespace service_manager
