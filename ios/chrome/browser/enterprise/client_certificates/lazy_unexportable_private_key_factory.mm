// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/lazy_unexportable_private_key_factory.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/memory/weak_ptr.h"
#import "base/task/thread_pool.h"
#import "base/values.h"
#import "components/enterprise/client_certificates/core/private_key_factory.h"
#import "components/enterprise/client_certificates/core/private_key_types.h"
#import "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"
#import "crypto/unexportable_key.h"
#import "ios/chrome/browser/enterprise/client_certificates/cert_utils.h"

namespace client_certificates {

namespace {
std::unique_ptr<PrivateKeyFactory> (*g_mock_factory_creator)() = nullptr;
}  // namespace

LazyUnexportablePrivateKeyFactory::LazyUnexportablePrivateKeyFactory(
    std::string_view application_tag)
    : application_tag_(application_tag) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LazyUnexportablePrivateKeyFactory::CreateBaseFactory,
                     application_tag_),
      base::BindOnce(&LazyUnexportablePrivateKeyFactory::OnBaseFactoryCreated,
                     weak_factory_.GetWeakPtr()));
}

LazyUnexportablePrivateKeyFactory::~LazyUnexportablePrivateKeyFactory() =
    default;

void LazyUnexportablePrivateKeyFactory::CreatePrivateKey(
    PrivateKeyCallback callback) {
  if (base_factory_) {
    base_factory_->CreatePrivateKey(std::move(callback));
    return;
  }
  pending_operations_.push_back(
      base::BindOnce(&LazyUnexportablePrivateKeyFactory::CreatePrivateKey,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LazyUnexportablePrivateKeyFactory::LoadPrivateKey(
    const client_certificates_pb::PrivateKey& serialized_private_key,
    PrivateKeyCallback callback) {
  if (base_factory_) {
    base_factory_->LoadPrivateKey(serialized_private_key, std::move(callback));
    return;
  }
  pending_operations_.push_back(base::BindOnce(
      &LazyUnexportablePrivateKeyFactory::LoadPrivateKey,
      weak_factory_.GetWeakPtr(), serialized_private_key, std::move(callback)));
}

void LazyUnexportablePrivateKeyFactory::LoadPrivateKeyFromDict(
    const base::DictValue& serialized_private_key,
    PrivateKeyCallback callback) {
  if (base_factory_) {
    base_factory_->LoadPrivateKeyFromDict(serialized_private_key,
                                          std::move(callback));
    return;
  }
  pending_operations_.push_back(
      base::BindOnce(&LazyUnexportablePrivateKeyFactory::LoadPrivateKeyFromDict,
                     weak_factory_.GetWeakPtr(), serialized_private_key.Clone(),
                     std::move(callback)));
}

// static
void LazyUnexportablePrivateKeyFactory::SetFactoryCreatorForTesting(
    std::unique_ptr<PrivateKeyFactory> (*func)()) {
  g_mock_factory_creator = func;
}

// static
std::unique_ptr<PrivateKeyFactory>
LazyUnexportablePrivateKeyFactory::CreateBaseFactory(
    std::string application_tag) {
  if (g_mock_factory_creator) {
    return g_mock_factory_creator();
  }

  PrivateKeyFactory::PrivateKeyFactoriesMap sub_factories;
  crypto::UnexportableKeyProvider::Config config;
  auto access_group = GetAccessGroup();
  if (access_group.has_value()) {
    config.keychain_access_group = access_group.value();
    if (!application_tag.empty()) {
      config.application_tag = application_tag;
    }

    auto unexportable_key_factory =
        UnexportablePrivateKeyFactory::TryCreate(std::move(config));
    if (unexportable_key_factory) {
      sub_factories.insert_or_assign(PrivateKeySource::kUnexportableKey,
                                     std::move(unexportable_key_factory));
    } else {
      LOG(ERROR) << "Failed to create unexportable key factory.";
    }
  }

  return PrivateKeyFactory::Create(std::move(sub_factories));
}

void LazyUnexportablePrivateKeyFactory::OnBaseFactoryCreated(
    std::unique_ptr<PrivateKeyFactory> factory) {
  base_factory_ = std::move(factory);
  for (auto& operation : pending_operations_) {
    std::move(operation).Run();
  }
  pending_operations_.clear();
}

}  // namespace client_certificates
