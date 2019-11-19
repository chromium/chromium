// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_SERVICE_H_
#define CONTENT_PUBLIC_TEST_TEST_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace content {

extern const char kTestServiceUrl[];

// Simple Service which provides a mojom::TestService impl. The service
// terminates itself after its TestService fulfills a single DoSomething call.
class TestService : public service_manager::Service, public mojom::TestService {
 public:
  explicit TestService(service_manager::mojom::ServiceRequest request);
  ~TestService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void Create(mojo::PendingReceiver<mojom::TestService> receiver);

  // TestService:
  void DoSomething(DoSomethingCallback callback) override;
  void DoTerminateProcess(DoTerminateProcessCallback callback) override;
  void DoCrashImmediately(DoCrashImmediatelyCallback callback) override;
  void CreateFolder(CreateFolderCallback callback) override;
  void GetRequestorName(GetRequestorNameCallback callback) override;
  void CreateSharedBuffer(const std::string& message,
                          CreateSharedBufferCallback callback) override;

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::Receiver<mojom::TestService> receiver_{this};

  // The name of the app connecting to us.
  std::string requestor_name_;

  DISALLOW_COPY_AND_ASSIGN(TestService);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_SERVICE_H_
