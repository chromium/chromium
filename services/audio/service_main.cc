// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/task/single_thread_task_executor.h"
#include "services/audio/service.h"
#include "services/audio/service_factory.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  base::SingleThreadTaskExecutor main_thread_task_executor;
  audio::CreateStandaloneService(std::make_unique<service_manager::BinderMap>(),
                                 std::move(request))
      ->RunUntilTermination();
}
