// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/known_legacy_scope_domains_delegate.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {
const std::string_view kPrefPath = "net.known_legacy_scope_domains";
}  // namespace

namespace network {

KnownLegacyScopeDomainsPrefDelegate::KnownLegacyScopeDomainsPrefDelegate(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service_);
}

KnownLegacyScopeDomainsPrefDelegate::~KnownLegacyScopeDomainsPrefDelegate() =
    default;

void KnownLegacyScopeDomainsPrefDelegate::RegisterPrefs(
    PrefRegistrySimple* pref_registry) {
  // Register the pref for the known legacy scope domains as a dictionary.
  pref_registry->RegisterDictionaryPref(kPrefPath);
}

const base::Value::Dict& KnownLegacyScopeDomainsPrefDelegate::GetLegacyDomains()
    const {
  return pref_service_->GetDict(kPrefPath);
}

void KnownLegacyScopeDomainsPrefDelegate::SetLegacyDomains(
    base::Value::Dict dict) {
  pref_service_->SetDict(kPrefPath, std::move(dict));
}

void KnownLegacyScopeDomainsPrefDelegate::WaitForPrefLoad(
    base::OnceClosure callback) {
  // If prefs haven't loaded yet, set up a pref init observer.
  if (pref_service_->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    pref_service_->AddPrefInitObserver(base::BindOnce(
        [](base::OnceClosure callback, bool) { std::move(callback).Run(); },
        std::move(callback)));
    return;
  }

  // If prefs have already loaded invoke the pref observer asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

bool KnownLegacyScopeDomainsPrefDelegate::IsPrefReady() {
  // Check if the pref has been loaded yet.
  auto status = pref_service_->GetInitializationStatus();
  return status == PrefService::INITIALIZATION_STATUS_SUCCESS ||
         status == PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}

}  // namespace network
