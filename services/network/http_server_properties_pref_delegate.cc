// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/http_server_properties_pref_delegate.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

const char kPrefPath[] = "net.http_server_properties";

namespace network {

HttpServerPropertiesPrefDelegate::HttpServerPropertiesPrefDelegate(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
}

HttpServerPropertiesPrefDelegate::~HttpServerPropertiesPrefDelegate() {}

void HttpServerPropertiesPrefDelegate::RegisterPrefs(
    PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterDictionaryPref(kPrefPath);
}

const base::Value::Dict& HttpServerPropertiesPrefDelegate::GetServerProperties()
    const {
  return pref_service_->GetDict(kPrefPath);
}

void HttpServerPropertiesPrefDelegate::SetServerProperties(
    base::Value::Dict dict,
    base::OnceClosure callback) {
  pref_service_->SetDict(kPrefPath, std::move(dict));
  if (callback)
    pref_service_->CommitPendingWrite(std::move(callback));
}

void HttpServerPropertiesPrefDelegate::WaitForPrefLoad(
    base::OnceClosure callback) {
  // If prefs haven't loaded yet, set up a pref init observer.
  if (pref_service_->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    pref_service_->AddPrefInitObserver(base::BindOnce(
        [](base::OnceClosure callback, bool) { std::move(callback).Run(); },
        std::move(callback)));
    return;
  }

  // If prefs have already loaded (currently doesn't happen), invoke the pref
  // observer asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

}  // namespace network
