// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/task/single_thread_task_executor.h"
#include "media/mojo/services/media_service_factory.h"
#include "services/service_manager/public/cpp/service_executable/service_main.h"
#include "services/service_manager/public/mojom/service.mojom.h"

void ServiceMain(service_manager::mojom::ServiceRequest request) {
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  base::SingleThreadTaskExecutor main_thread_task_executor;
  media::CreateMediaServiceForTesting(std::move(request))
      ->RunUntilTermination();
}
