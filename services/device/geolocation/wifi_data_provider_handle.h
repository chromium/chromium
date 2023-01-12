// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A wifi data provider provides wifi data from the device that is used by a
// NetworkLocationProvider to obtain a position fix. We use a singleton
// instance of the wifi data provider manager, which is used by multiple
// NetworkLocationProvider objects.
//
// This file provides WifiDataProviderHandle, which provides static methods to
// access the singleton instance. The singleton instance uses a private
// implementation of WifiDataProvider to abstract across platforms and also to
// allow mock providers to be used for testing.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_HANDLE_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_HANDLE_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "services/device/geolocation/wifi_data.h"

namespace device {

class WifiDataProvider;

// A handle for using wifi data providers.
//
// We use a singleton instance of WifiDataProvider which is shared by multiple
// network location providers.
class WifiDataProviderHandle {
 public:
  typedef WifiDataProvider* (*ImplFactoryFunction)();
  typedef base::RepeatingClosure WifiDataUpdateCallback;

  // Sets the factory function which will be used by Register to create the
  // implementation used by the singleton instance. This factory approach is
  // used both to abstract across platform-specific implementations and to
  // inject mock implementations for testing.
  static void SetFactoryForTesting(ImplFactoryFunction factory_function_in);

  // Resets the factory function to the default.
  static void ResetFactoryForTesting();

  // Creates a handle on WifiDataProvider.
  static std::unique_ptr<WifiDataProviderHandle> CreateHandle(
      WifiDataUpdateCallback* callback);

  ~WifiDataProviderHandle();

  WifiDataProviderHandle(const WifiDataProviderHandle&) = delete;
  WifiDataProviderHandle& operator=(const WifiDataProviderHandle&) = delete;

  // Returns true if the data provider is currently delayed by polling policy.
  bool DelayedByPolicy();

  // Provides whatever data the provider has, which may be nothing. Return
  // value indicates whether this is all the data the provider could ever
  // obtain.
  bool GetData(WifiData* data);

  void ForceRescan();

 private:
  explicit WifiDataProviderHandle(WifiDataUpdateCallback* callback);

  void AddCallback(WifiDataUpdateCallback* callback);
  bool RemoveCallback(WifiDataUpdateCallback* callback);
  bool has_callbacks() const;

  void StartDataProvider();
  void StopDataProvider();

  static WifiDataProvider* DefaultFactoryFunction();

  static scoped_refptr<WifiDataProvider> GetOrCreateProvider();

  // The factory function used to create the singleton instance.
  static ImplFactoryFunction factory_function_;

  scoped_refptr<WifiDataProvider> impl_;
  raw_ptr<WifiDataUpdateCallback> callback_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_HANDLE_H_
