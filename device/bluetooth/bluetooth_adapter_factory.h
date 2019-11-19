// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

#if defined(OS_CHROMEOS)
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace device {

// A factory class for building a Bluetooth adapter on platforms where Bluetooth
// is available.
//
// Testing: Clients that want to specify their own return values for
// BluetoothAdapterFactory's functions need to call InitValuesForTesting().
// If this function has been called, the Factory will return the specified
// test values instead of the default values.
//
// Only IsLowEnergySupported uses ValuesForTesting.
// TODO(crbug.com/569709): Use ValuesForTesting for all functions.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterFactory {
 public:
  using AdapterCallback =
      base::OnceCallback<void(scoped_refptr<BluetoothAdapter> adapter)>;

#if defined(OS_CHROMEOS)
  using BleScanParserCallback = base::RepeatingCallback<
      mojo::PendingRemote<data_decoder::mojom::BleScanParser>()>;
#endif  // defined(OS_CHROMEOS)

  ~BluetoothAdapterFactory();

  static BluetoothAdapterFactory& Get();

  // Returns true if the platform supports Bluetooth. It does not imply that
  // there is a Bluetooth radio. Use BluetoothAdapter::IsPresent to know
  // if there is a Bluetooth radio present.
  static bool IsBluetoothSupported();

  // Returns true if the platform supports Bluetooth Low Energy. This is
  // independent of whether or not there is a Bluetooth radio present e.g.
  // Windows 7 does not support BLE so IsLowEnergySupported would return
  // false. Windows 10, on the other hand, supports BLE so this function
  // returns true even if there is no Bluetooth radio on the system.
  bool IsLowEnergySupported();

  // Returns the shared instance of the default adapter, creating and
  // initializing it if necessary. |callback| is called with the adapter
  // instance passed only once the adapter is fully initialized and ready to
  // use.
  static void GetAdapter(AdapterCallback callback);

  // Returns the shared instance of the classic adapter, creating and
  // initializing it if necessary. |callback| is called with the adapter
  // instance passed only once the adapter is fully initialized and ready to
  // use.
  // For all platforms except Windows this is equivalent to calling
  // GetAdapter(), as the default adapter already supports Bluetooth classic.
  static void GetClassicAdapter(AdapterCallback callback);

#if defined(OS_LINUX)
  // Calls |BluetoothAdapter::Shutdown| on the adapter if
  // present.
  static void Shutdown();
#endif

  // Sets the shared instance of the default adapter for testing purposes only,
  // no reference is retained after completion of the call, removing the last
  // reference will reset the factory.
  static void SetAdapterForTesting(scoped_refptr<BluetoothAdapter> adapter);

  // Returns true iff the implementation has a (non-NULL) shared instance of the
  // adapter. Exposed for testing.
  static bool HasSharedInstanceForTesting();

#if defined(OS_CHROMEOS)
  // Sets the mojo::Remote<BleScanParser> callback used in Get*() below.
  static void SetBleScanParserCallback(BleScanParserCallback callback);
  // Returns a reference to a parser for BLE advertisement packets.
  // This will be an empty callback until something calls Set*() above.
  static BleScanParserCallback GetBleScanParserCallback();
#endif  // defined(OS_CHROMEOS)

  // ValuestForTesting holds the return values for BluetoothAdapterFactory's
  // functions that have been set for testing.
  class DEVICE_BLUETOOTH_EXPORT GlobalValuesForTesting {
   public:
    GlobalValuesForTesting();
    ~GlobalValuesForTesting();

    void SetLESupported(bool supported) { le_supported_ = supported; }

    bool GetLESupported() { return le_supported_; }

    base::WeakPtr<GlobalValuesForTesting> GetWeakPtr();

   private:
    bool le_supported_ = false;

    base::WeakPtrFactory<GlobalValuesForTesting> weak_ptr_factory_{this};
    DISALLOW_COPY_AND_ASSIGN(GlobalValuesForTesting);
  };

  // Returns an object that clients can use to control the return values
  // of the Factory's functions. BluetoothAdapterFactory will keep a WeakPtr
  // to this object so clients can just destroy the returned
  // GlobalValuesForTesting to reset BluetoothAdapterFactory's returned
  // values once they are done.
  //
  // Sometimes clients cannot guarantee that whey will reset all the values
  // before another clients starts interacting with BluetoothAdapterFactory.
  // By passing ownership of GlobalValuesForTesting to the clients, we
  // ensure that only the last client that called
  // InitGlobalValuesForTesting() will modify BluetoothAdapterFactory's
  // returned values.
  std::unique_ptr<GlobalValuesForTesting> InitGlobalValuesForTesting();

 private:
  // Friend LazyInstance to permit access to private constructor.
  friend base::LazyInstanceTraitsBase<BluetoothAdapterFactory>;

  BluetoothAdapterFactory();

  base::WeakPtr<GlobalValuesForTesting> values_for_testing_;

#if defined(OS_CHROMEOS)
  BleScanParserCallback ble_scan_parser_;
#endif  // defined(OS_CHROMEOS)
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
