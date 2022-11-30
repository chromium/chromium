// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_service_manager.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/background_service_manager.h"

namespace service_manager {

TestServiceManager::TestServiceManager()
    : TestServiceManager(std::vector<Manifest>()) {}

TestServiceManager::TestServiceManager(const std::vector<Manifest>& manifests)
    : background_service_manager_(
          std::make_unique<BackgroundServiceManager>(manifests)) {}

TestServiceManager::~TestServiceManager() = default;

mojo::PendingReceiver<mojom::Service> TestServiceManager::RegisterTestInstance(
    const std::string& service_name) {
  return RegisterInstance(Identity{service_name, base::Token::CreateRandom(),
                                   base::Token{}, base::Token::CreateRandom()});
}

mojo::PendingReceiver<mojom::Service> TestServiceManager::RegisterInstance(
    const Identity& identity) {
  mojo::PendingRemote<mojom::Service> service;
  auto receiver = service.InitWithNewPipeAndPassReceiver();
  background_service_manager_->RegisterService(identity, std::move(service),
                                               mojo::NullReceiver());
  return receiver;
}

}  // namespace service_manager
