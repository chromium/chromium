// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_service.h"

#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/services/font/font_service_app.h"

namespace content {

namespace {

base::SequencedTaskRunner* GetServiceTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner{base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_BLOCKING})};
  return task_runner->get();
}

void BindToBackgroundFontService(
    mojo::PendingReceiver<font_service::mojom::FontService> receiver) {
  static base::NoDestructor<font_service::FontServiceApp> service;
  service->BindReceiver(std::move(receiver));
}

}  // namespace

void ConnectToFontService(
    mojo::PendingReceiver<font_service::mojom::FontService> receiver) {
  GetServiceTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BindToBackgroundFontService, std::move(receiver)));
}

}  // namespace content
