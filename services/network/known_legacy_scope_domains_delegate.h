// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_KNOWN_LEGACY_SCOPE_DOMAINS_DELEGATE_H_
#define SERVICES_NETWORK_KNOWN_LEGACY_SCOPE_DOMAINS_DELEGATE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/cookies/cookie_monster.h"

class PrefRegistrySimple;
class PrefService;

namespace network {

// Pref used in net::CookieMonster to help determine which cookie domains
// are currently under legacy mode.
class COMPONENT_EXPORT(NETWORK_SERVICE) KnownLegacyScopeDomainsPrefDelegate
    : public net::CookieMonster::PrefDelegate {
 public:
  explicit KnownLegacyScopeDomainsPrefDelegate(PrefService* pref_service);

  KnownLegacyScopeDomainsPrefDelegate(
      const KnownLegacyScopeDomainsPrefDelegate&) = delete;
  KnownLegacyScopeDomainsPrefDelegate& operator=(
      const KnownLegacyScopeDomainsPrefDelegate&) = delete;

  ~KnownLegacyScopeDomainsPrefDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* pref_registry);

  // net::CookieMonster::PrefDelegate implementations.
  const base::Value::Dict& GetLegacyDomains() const override;
  void SetLegacyDomains(base::Value::Dict dict) override;
  void WaitForPrefLoad(base::OnceClosure callback) override;

  bool IsPrefReady() override;

 private:
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_KNOWN_LEGACY_SCOPE_DOMAINS_DELEGATE_H_
