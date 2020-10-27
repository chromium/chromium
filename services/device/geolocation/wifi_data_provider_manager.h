// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A wifi data provider provides wifi data from the device that is used by a
// NetworkLocationProvider to obtain a position fix. We use a singleton
// instance of the wifi data provider manager, which is used by multiple
// NetworkLocationProvider objects.
//
// This file provides WifiDataProviderManager, which provides static methods to
// access the singleton instance. The singleton instance uses a private
// implementation of WifiDataProvider to abstract across platforms and also to
// allow mock providers to be used for testing.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_MANAGER_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_MANAGER_H_

#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "services/device/geolocation/wifi_data.h"

namespace device {

class WifiDataProvider;

// A manager for wifi data providers.
//
// We use a singleton instance of this class which is shared by multiple network
// location providers. These location providers access the instance through the
// Register and Unregister methods.
class WifiDataProviderManager {
 public:
  typedef WifiDataProvider* (*ImplFactoryFunction)(void);

  // Sets the factory function which will be used by Register to create the
  // implementation used by the singleton instance. This factory approach is
  // used both to abstract accross platform-specific implementations and to
  // inject mock implementations for testing.
  static void SetFactoryForTesting(ImplFactoryFunction factory_function_in);

  // Resets the factory function to the default.
  static void ResetFactoryForTesting();

  typedef base::RepeatingClosure WifiDataUpdateCallback;

  // Registers a callback, which will be run whenever new data is available.
  // Instantiates the singleton if necessary, and always returns it.
  static WifiDataProviderManager* Register(WifiDataUpdateCallback* callback);

  // Removes a callback. If this is the last callback, deletes the singleton
  // instance. Return value indicates success.
  static bool Unregister(WifiDataUpdateCallback* callback);

  // Returns true if the data provider is currently delayed by polling policy.
  bool DelayedByPolicy();

  // Provides whatever data the provider has, which may be nothing. Return
  // value indicates whether this is all the data the provider could ever
  // obtain.
  bool GetData(WifiData* data);

 private:
  // Private constructor and destructor, callers access singleton through
  // Register and Unregister.
  WifiDataProviderManager();
  ~WifiDataProviderManager();

  void AddCallback(WifiDataUpdateCallback* callback);
  bool RemoveCallback(WifiDataUpdateCallback* callback);
  bool has_callbacks() const;

  void StartDataProvider();
  void StopDataProvider();

  static WifiDataProvider* DefaultFactoryFunction();

  // The singleton-like instance of this class. (Not 'true' singleton, as it
  // may go through multiple create/destroy/create cycles per process instance,
  // e.g. when under test).
  static WifiDataProviderManager* instance_;

  // The factory function used to create the singleton instance.
  static ImplFactoryFunction factory_function_;

  // The internal implementation.
  scoped_refptr<WifiDataProvider> impl_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProviderManager);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_MANAGER_H_
