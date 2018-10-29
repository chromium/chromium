// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/video_capture/service_impl.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  return service_manager::ServiceRunner(
             video_capture::ServiceImpl::Create().release())
      .Run(service_request_handle);
}
