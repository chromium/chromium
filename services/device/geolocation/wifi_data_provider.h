// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_H_
#define SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_H_

#include <set>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/geolocation/wifi_data.h"

namespace device {

class WifiDataProvider : public base::RefCountedThreadSafe<WifiDataProvider> {
 public:
  WifiDataProvider();

  WifiDataProvider(const WifiDataProvider&) = delete;
  WifiDataProvider& operator=(const WifiDataProvider&) = delete;

  // Tells the provider to start looking for data. Callbacks will start
  // receiving notifications after this call.
  virtual void StartDataProvider() = 0;

  // Tells the provider to stop looking for data. Callbacks will stop
  // receiving notifications after this call.
  virtual void StopDataProvider() = 0;

  // Returns true if the provider is delayed due to scanning policy.
  virtual bool DelayedByPolicy() = 0;

  // Provides whatever data the provider has, which may be nothing. Return
  // value indicates whether this is all the data the provider could ever
  // obtain.
  virtual bool GetData(WifiData* data) = 0;

  virtual void ForceRescan() = 0;

  typedef base::RepeatingClosure WifiDataUpdateCallback;

  void AddCallback(WifiDataUpdateCallback* callback);

  bool RemoveCallback(WifiDataUpdateCallback* callback);

  bool has_callbacks() const;

  base::WeakPtr<WifiDataProvider> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  friend class base::RefCountedThreadSafe<WifiDataProvider>;
  virtual ~WifiDataProvider();

  typedef std::set<WifiDataUpdateCallback*> CallbackSet;

  // Runs all callbacks via a posted task, so we can unwind callstack here and
  // avoid client reentrancy.
  void RunCallbacks();

  bool CalledOnClientThread() const;

  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner() const {
    return client_task_runner_;
  }

 private:
  void DoRunCallbacks();

  // The task runner for the client thread, all callbacks should run on it.
  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  CallbackSet callbacks_;

  base::WeakPtrFactory<WifiDataProvider> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GEOLOCATION_WIFI_DATA_PROVIDER_H_
