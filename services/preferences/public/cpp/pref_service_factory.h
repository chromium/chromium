// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a way for any service to connect to the pref service to access
// the application's current preferences.

// Access is provided through a synchronous interface, exposed using the
// |PrefService| class.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_FACTORY_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/token.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/preferences/public/mojom/preferences.mojom.h"

class PrefRegistry;
class PrefService;

namespace service_manager {
class Connector;
}

namespace prefs {

// Note that |PrefService| might not be fully initialized yet and thus you need
// to call |AddPrefInitObserver| on it before using it. Passed |nullptr| on
// failure.
using ConnectCallback = base::Callback<void(std::unique_ptr<::PrefService>)>;

// Create a |PrefService| object acting as a client library for the pref
// service, using the provided |connector|. Connecting is asynchronous and
// |callback| will be called when it has been established. All preferences that
// will be accessed need to be registered in |pref_registry| first. If provided
// |client_token| uniquely identifies the client, fixing it to a specific set of
// observed prefs; if not provided, the Service Manager Identity used to acquire
// |connector| will be used for that purpose instead.
void ConnectToPrefService(
    mojo::PendingRemote<mojom::PrefStoreConnector> connector,
    scoped_refptr<PrefRegistry> pref_registry,
    base::Optional<base::Token> client_token,
    ConnectCallback callback);

// Create a |PrefService| object acting as a client library for the pref
// service, by connecting to the service using |connector|. Connecting is
// asynchronous and |callback| will be called when it has been established. All
// preferences that will be accessed need to be registered in |pref_registry|
// first.
void ConnectToPrefService(
    service_manager::Connector* connector,
    scoped_refptr<PrefRegistry> pref_registry,
    ConnectCallback callback,
    base::StringPiece service_name = mojom::kServiceName);

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_PREF_SERVICE_FACTORY_H_
