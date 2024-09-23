// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_

#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace content {

class WebBluetoothServiceImpl;

// Wrapper around BluetoothAdapterFactory that allows us to change
// the underlying BluetoothAdapter object and have the observers
// observe the new instance of the object.
// TODO(ortuno): Once there is no need to swap the adapter to change its
// behavior observers should add/remove themselves to/from the adapter.
// http://crbug.com/603291
class CONTENT_EXPORT BluetoothAdapterFactoryWrapper {
 public:
  using AcquireAdapterCallback =
      base::OnceCallback<void(scoped_refptr<device::BluetoothAdapter>)>;

  BluetoothAdapterFactoryWrapper();
  BluetoothAdapterFactoryWrapper(const BluetoothAdapterFactoryWrapper& other) =
      delete;
  BluetoothAdapterFactoryWrapper& operator=(
      const BluetoothAdapterFactoryWrapper& other) = delete;
  ~BluetoothAdapterFactoryWrapper();

  static BluetoothAdapterFactoryWrapper& Get();

  // Returns true if the platform supports Bluetooth Low Energy or if
  // SetBluetoothAdapterOverride has been called.
  bool IsLowEnergySupported();

  // Adds |service| to the set of adapter observers if this is the first time
  // the service has tried to acquire the adapter. If another service has
  // acquired the adapter in the past it adds |service| as an observer to that
  // adapter, otherwise it gets a new adapter and adds |service| to it. Runs
  // |callback| with the adapter |service| has been added to.
  void AcquireAdapter(WebBluetoothServiceImpl* service,
                      AcquireAdapterCallback callback);
  // Removes |service| from the list of adapter observers if |service|
  // has acquired the adapter in the past. If there are no more observers
  // it deletes the reference to the adapter.
  void ReleaseAdapter(WebBluetoothServiceImpl* service);

  // Returns an adapter if |service| has acquired an adapter in the past and
  // this instance holds a reference to an adapter. Otherwise returns nullptr.
  device::BluetoothAdapter* GetAdapter(WebBluetoothServiceImpl* service);

  // Sets a new BluetoothAdapter to be returned by GetAdapter. When setting
  // a new adapter all observers from the old adapter are removed and added
  // to |test_adapter|.
  void SetBluetoothAdapterOverride(
      scoped_refptr<device::BluetoothAdapter> test_adapter);

 private:
  friend class WebBluetoothServiceImplTestWithBaseAdapter;

  void OnGetAdapter(AcquireAdapterCallback continuation,
                    scoped_refptr<device::BluetoothAdapter> adapter);
  void OnGetOverrideAdapter(
      AcquireAdapterCallback continuation,
      scoped_refptr<device::BluetoothAdapter> override_adapter);

  bool HasAdapter(WebBluetoothServiceImpl* service);
  void MaybeAddAdapterObserver(WebBluetoothServiceImpl* service);
  void RemoveAdapterObserver(WebBluetoothServiceImpl* service);
  scoped_refptr<device::BluetoothAdapter> GetActiveAdapter();

  // Sets |adapter_| to a BluetoothAdapter instance and register observers,
  // releasing references to previous |adapter_|.
  void SetAdapterInternal(scoped_refptr<device::BluetoothAdapter> adapter,
                          bool is_override_adapter);

  // A BluetoothAdapter instance representing an adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // A BluetoothAdapter override instance configured for testing purposes.
  scoped_refptr<device::BluetoothAdapter> override_adapter_;

  // A BluetoothAdapter instance configured for testing purposes which will be
  // activated by the first call to AcquireAdapter(). A pending override adapter
  // is required to support graceful removal of an already supplied override
  // adapter.
  scoped_refptr<device::BluetoothAdapter> pending_override_adapter_;

  // We keep a list of all observers so that when the adapter gets swapped,
  // we can remove all observers from the old adapter and add them to the
  // new adapter.
  std::unordered_set<raw_ptr<WebBluetoothServiceImpl, CtnExperimental>>
      adapter_observers_;

  // Should only be called on the UI thread.
  THREAD_CHECKER(thread_checker_);

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterFactoryWrapper> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_
