// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom-forward.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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
// TODO(crbug.com/40083385): Use ValuesForTesting for all functions.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterFactory {
 public:
  using AdapterCallback =
      base::OnceCallback<void(scoped_refptr<BluetoothAdapter> adapter)>;

#if BUILDFLAG(IS_CHROMEOS)
  using BleScanParserCallback = base::RepeatingCallback<
      mojo::PendingRemote<data_decoder::mojom::BleScanParser>()>;
#endif  // BUILDFLAG(IS_CHROMEOS)

  BluetoothAdapterFactory();
  ~BluetoothAdapterFactory();

  static BluetoothAdapterFactory* Get();

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
  void GetAdapter(AdapterCallback callback);

  // Returns the shared instance of the classic adapter, creating and
  // initializing it if necessary. |callback| is called with the adapter
  // instance passed only once the adapter is fully initialized and ready to
  // use.
  // For all platforms except Windows this is equivalent to calling
  // GetAdapter(), as the default adapter already supports Bluetooth classic.
  void GetClassicAdapter(AdapterCallback callback);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(IS_CHROMEOS)
  // Sets the mojo::Remote<BleScanParser> callback used in Get*() below.
  static void SetBleScanParserCallback(BleScanParserCallback callback);
  // Returns a reference to a parser for BLE advertisement packets.
  // This will be an empty callback until something calls Set*() above.
  static BleScanParserCallback GetBleScanParserCallback();
#endif  // BUILDFLAG(IS_CHROMEOS)

  // GlobalOverrideValues holds the return values for BluetoothAdapterFactory's
  // functions that have been set for testing or for simulated devices.
  class DEVICE_BLUETOOTH_EXPORT GlobalOverrideValues {
   public:
    GlobalOverrideValues();

    GlobalOverrideValues(const GlobalOverrideValues&) = delete;
    GlobalOverrideValues& operator=(const GlobalOverrideValues&) = delete;

    ~GlobalOverrideValues();

    void SetLESupported(bool supported) { le_supported_ = supported; }

    bool GetLESupported() { return le_supported_; }

    base::WeakPtr<GlobalOverrideValues> GetWeakPtr();

   private:
    bool le_supported_ = false;

    base::WeakPtrFactory<GlobalOverrideValues> weak_ptr_factory_{this};
  };

  // Returns an object that clients can use to control the return values
  // of the Factory's functions. BluetoothAdapterFactory will keep a WeakPtr
  // to this object so clients can just destroy the returned
  // GlobalOverrideValues to reset BluetoothAdapterFactory's returned
  // values once they are done.
  //
  // Sometimes clients cannot guarantee that whey will reset all the values
  // before another clients starts interacting with BluetoothAdapterFactory.
  // By passing ownership of GlobalOverrideValues to the clients, we
  // ensure that only the last client that called
  // InitGlobalOverrideValues() will modify BluetoothAdapterFactory's
  // returned values.
  std::unique_ptr<GlobalOverrideValues> InitGlobalOverrideValues();

 private:
  FRIEND_TEST_ALL_PREFIXES(
      SerialPortManagerImplTest,
      BluetoothSerialDeviceEnumerator_DeleteBeforeAdapterInit);

  void AdapterInitialized();
#if BUILDFLAG(IS_WIN)
  void ClassicAdapterInitialized();
#endif

  base::WeakPtr<GlobalOverrideValues> override_values_;

  // While a new BluetoothAdapter is being initialized the factory retains a
  // reference to it. After initialization is complete |adapter_callbacks_|
  // are run and, to allow the class to be destroyed if nobody is using it,
  // that reference is dropped and |adapter_| is used instead.
  scoped_refptr<BluetoothAdapter> adapter_under_initialization_;
  std::vector<AdapterCallback> adapter_callbacks_;
  base::WeakPtr<BluetoothAdapter> adapter_;

#if BUILDFLAG(IS_WIN)
  // On Windows different implementations of BluetoothAdapter are used for
  // supporting Classic and Low Energy devices. The factory logic is duplicated.
  scoped_refptr<BluetoothAdapter> classic_adapter_under_initialization_;
  std::vector<AdapterCallback> classic_adapter_callbacks_;
  base::WeakPtr<BluetoothAdapter> classic_adapter_;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  BleScanParserCallback ble_scan_parser_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_H_
