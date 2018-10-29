// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/unzip/unzip_service_creator.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/unzip/public/interfaces/constants.mojom.h"
#include "components/services/unzip/unzip_service.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"

void RegisterUnzipService(web::BrowserState::StaticServiceMap* services) {
  service_manager::EmbeddedServiceInfo unzip_info;
  unzip_info.factory = base::BindRepeating(&unzip::UnzipService::CreateService);
  unzip_info.task_runner = base::ThreadTaskRunnerHandle::Get();
  services->insert(std::make_pair(unzip::mojom::kServiceName, unzip_info));
}
