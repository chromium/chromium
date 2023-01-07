// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  base::SingleThreadTaskExecutor main_task_executor;
  service_manager::Service service;
  service_manager::ServiceReceiver service_receiver(&service,
                                                    std::move(receiver));
  service.RunUntilTermination();
}
