// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_

#include <unordered_set>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// Wrapper around BluetoothAdapterFactory that allows us to change
// the underlying BluetoothAdapter object and have the observers
// observe the new instance of the object.
// TODO(ortuno): Once there is no need to swap the adapter to change its
// behavior observers should add/remove themselves to/from the adapter.
// http://crbug.com/603291
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterFactoryWrapper {
 public:
  using AcquireAdapterCallback =
      base::OnceCallback<void(scoped_refptr<BluetoothAdapter>)>;

  ~BluetoothAdapterFactoryWrapper();

  static BluetoothAdapterFactoryWrapper& Get();

  // Returns true if the platform supports Bluetooth Low Energy or if
  // SetBluetoothAdapterForTesting has been called.
  bool IsLowEnergySupported();

  // Adds |observer| to the set of adapter observers. If another observer has
  // acquired the adapter in the past it adds |observer| as an observer to that
  // adapter, otherwise it gets a new adapter and adds |observer| to it. Runs
  // |callback| with the adapter |observer| has been added to.
  void AcquireAdapter(BluetoothAdapter::Observer* observer,
                      AcquireAdapterCallback callback);
  // Removes |observer| from the list of adapter observers if |observer|
  // has acquired the adapter in the past. If there are no more observers
  // it deletes the reference to the adapter.
  void ReleaseAdapter(BluetoothAdapter::Observer* observer);

  // Returns an adapter if |observer| has acquired an adapter in the past and
  // this instance holds a reference to an adapter. Otherwise returns nullptr.
  BluetoothAdapter* GetAdapter(BluetoothAdapter::Observer* observer);

  // Sets a new BluetoothAdapter to be returned by GetAdapter. When setting
  // a new adapter all observers from the old adapter are removed and added
  // to |mock_adapter|.
  void SetBluetoothAdapterForTesting(
      scoped_refptr<BluetoothAdapter> mock_adapter);

 private:
  // friend LazyInstance to permit access to private constructor.
  friend base::LazyInstanceTraitsBase<BluetoothAdapterFactoryWrapper>;

  BluetoothAdapterFactoryWrapper();

  void OnGetAdapter(AcquireAdapterCallback continuation,
                    scoped_refptr<BluetoothAdapter> adapter);

  bool HasAdapter(BluetoothAdapter::Observer* observer);
  void AddAdapterObserver(BluetoothAdapter::Observer* observer);
  void RemoveAdapterObserver(BluetoothAdapter::Observer* observer);

  // Sets |adapter_| to a BluetoothAdapter instance and register observers,
  // releasing references to previous |adapter_|.
  void set_adapter(scoped_refptr<BluetoothAdapter> adapter);

  // A BluetoothAdapter instance representing an adapter of the system.
  scoped_refptr<BluetoothAdapter> adapter_;

  // We keep a list of all observers so that when the adapter gets swapped,
  // we can remove all observers from the old adapter and add them to the
  // new adapter.
  std::unordered_set<BluetoothAdapter::Observer*> adapter_observers_;

  // Should only be called on the UI thread.
  base::ThreadChecker thread_checker_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterFactoryWrapper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterFactoryWrapper);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_
