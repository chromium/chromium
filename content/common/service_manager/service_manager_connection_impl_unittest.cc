// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_manager/service_manager_connection_impl.h"

#include "base/bind_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

constexpr char kTestServiceName[] = "test service";

}  // namespace

TEST(ServiceManagerConnectionImplTest, ServiceLaunchThreading) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::Thread io_thread("ServiceManagerConnectionImplTest IO Thread");
  io_thread.Start();
  service_manager::mojom::ServicePtr service;
  ServiceManagerConnectionImpl connection_impl(mojo::MakeRequest(&service),
                                               io_thread.task_runner());
  ServiceManagerConnection& connection = connection_impl;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  connection.AddServiceRequestHandler(
      kTestServiceName, base::BindLambdaForTesting(
                            [&event](service_manager::mojom::ServiceRequest) {
                              event.Signal();
                            }));
  connection.Start();

  mojo::PendingRemote<service_manager::mojom::Service> packaged_service;
  mojo::PendingRemote<service_manager::mojom::ProcessMetadata> metadata;
  ignore_result(metadata.InitWithNewPipeAndPassReceiver());
  service->CreatePackagedServiceInstance(
      service_manager::Identity(kTestServiceName, base::Token::CreateRandom(),
                                base::Token(), base::Token::CreateRandom()),
      packaged_service.InitWithNewPipeAndPassReceiver(), std::move(metadata));
  event.Wait();
}

}  // namespace content
