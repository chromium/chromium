// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_USER_ID_USER_ID_SERVICE_H_
#define SERVICES_USER_ID_USER_ID_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/test/user_id/public/mojom/user_id.mojom.h"

namespace user_id {

class UserIdService : public service_manager::Service, public mojom::UserId {
 public:
  explicit UserIdService(service_manager::mojom::ServiceRequest request);
  ~UserIdService() override;

 private:
  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::UserId:
  void GetInstanceGroup(GetInstanceGroupCallback callback) override;

  void BindUserIdReceiver(mojo::PendingReceiver<mojom::UserId> receiver);

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;
  mojo::ReceiverSet<mojom::UserId> receivers_;

  DISALLOW_COPY_AND_ASSIGN(UserIdService);
};

}  // namespace user_id

#endif  // SERVICES_USER_ID_USER_ID_SERVICE_H_
