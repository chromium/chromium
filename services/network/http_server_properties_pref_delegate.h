// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_HTTP_SERVER_PROPERTIES_PREF_DELEGATE_H_
#define SERVICES_NETWORK_HTTP_SERVER_PROPERTIES_PREF_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/http/http_server_properties.h"

class PrefRegistrySimple;

namespace network {

// Manages disk storage for a net::HttpServerPropertiesManager.
class HttpServerPropertiesPrefDelegate
    : public net::HttpServerProperties::PrefDelegate {
 public:
  // The created object must be destroyed before |pref_service|.
  explicit HttpServerPropertiesPrefDelegate(PrefService* pref_service);

  HttpServerPropertiesPrefDelegate(const HttpServerPropertiesPrefDelegate&) =
      delete;
  HttpServerPropertiesPrefDelegate& operator=(
      const HttpServerPropertiesPrefDelegate&) = delete;

  ~HttpServerPropertiesPrefDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* pref_registry);

  // net::HttpServerProperties::PrefDelegate implementation.
  const base::Value::Dict& GetServerProperties() const override;
  void SetServerProperties(base::Value::Dict dict,
                           base::OnceClosure callback) override;
  void WaitForPrefLoad(base::OnceClosure callback) override;

 private:
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_HTTP_SERVER_PROPERTIES_PREF_DELEGATE_H_
