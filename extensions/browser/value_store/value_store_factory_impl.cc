// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_factory_impl.h"

#include "base/memory/scoped_refptr.h"
#include "extensions/browser/value_store/legacy_value_store_factory.h"

namespace extensions {

using SettingsNamespace = settings_namespace::Namespace;

ValueStoreFactoryImpl::ValueStoreFactoryImpl(const base::FilePath& profile_path)
    : legacy_factory_(
          base::MakeRefCounted<LegacyValueStoreFactory>(profile_path)) {}

ValueStoreFactoryImpl::~ValueStoreFactoryImpl() = default;

std::unique_ptr<ValueStore> ValueStoreFactoryImpl::CreateRulesStore() {
  return legacy_factory_->CreateRulesStore();
}

std::unique_ptr<ValueStore> ValueStoreFactoryImpl::CreateStateStore() {
  return legacy_factory_->CreateStateStore();
}

std::unique_ptr<ValueStore> ValueStoreFactoryImpl::CreateSettingsStore(
    SettingsNamespace settings_namespace,
    ModelType model_type,
    const ExtensionId& extension_id) {
  return legacy_factory_->CreateSettingsStore(settings_namespace, model_type,
                                              extension_id);
}

void ValueStoreFactoryImpl::DeleteSettings(SettingsNamespace settings_namespace,
                                           ModelType model_type,
                                           const ExtensionId& extension_id) {
  legacy_factory_->DeleteSettings(settings_namespace, model_type, extension_id);
}

bool ValueStoreFactoryImpl::HasSettings(SettingsNamespace settings_namespace,
                                        ModelType model_type,
                                        const ExtensionId& extension_id) {
  return legacy_factory_->HasSettings(settings_namespace, model_type,
                                      extension_id);
}

std::set<ExtensionId> ValueStoreFactoryImpl::GetKnownExtensionIDs(
    SettingsNamespace settings_namespace,
    ModelType model_type) const {
  return legacy_factory_->GetKnownExtensionIDs(settings_namespace, model_type);
}

}  // namespace extensions
