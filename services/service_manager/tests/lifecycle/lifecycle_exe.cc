// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/lifecycle/app_client.h"

MojoResult ServiceMain(MojoHandle service_request_handle) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  service_manager::test::AppClient app_client(
      service_manager::mojom::ServiceRequest(mojo::MakeScopedHandle(
          mojo::MessagePipeHandle(service_request_handle))),
      run_loop.QuitClosure());
  run_loop.Run();
  return MOJO_RESULT_OK;
}
