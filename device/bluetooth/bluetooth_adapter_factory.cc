// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#endif
#if defined(ANDROID)
#include "base/android/build_info.h"
#endif

namespace device {

namespace {

static base::LazyInstance<BluetoothAdapterFactory>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

// Shared default adapter instance.  We don't want to keep this class around
// if nobody is using it, so use a WeakPtr and create the object when needed.
// Since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors, we use a leaky lazy instance for it.
base::LazyInstance<base::WeakPtr<BluetoothAdapter>>::Leaky default_adapter =
    LAZY_INSTANCE_INITIALIZER;

#if defined(OS_WIN) || defined(OS_LINUX)
typedef std::vector<BluetoothAdapterFactory::AdapterCallback>
    AdapterCallbackList;

// List of adapter callbacks to be called once the adapter is initialized.
// Since Google C++ Style (and clang's static analyzer) forbids us having
// exit-time destructors we use a lazy instance for it.
base::LazyInstance<AdapterCallbackList>::DestructorAtExit adapter_callbacks =
    LAZY_INSTANCE_INITIALIZER;

void RunAdapterCallbacks() {
  DCHECK(default_adapter.Get());
  scoped_refptr<BluetoothAdapter> adapter(default_adapter.Get().get());
  for (auto& callback : adapter_callbacks.Get())
    std::move(callback).Run(adapter);

  adapter_callbacks.Get().clear();
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_WIN)
// Shared classic adapter instance. See above why this is a lazy instance.
// Note: This is only applicable on Windows, as here the default adapter does
// not provide Bluetooth Classic support yet.
base::LazyInstance<base::WeakPtr<BluetoothAdapter>>::Leaky classic_adapter =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<AdapterCallbackList>::DestructorAtExit
    classic_adapter_callbacks = LAZY_INSTANCE_INITIALIZER;

void RunClassicAdapterCallbacks() {
  DCHECK(classic_adapter.Get());
  scoped_refptr<BluetoothAdapter> adapter(classic_adapter.Get().get());
  for (auto& callback : classic_adapter_callbacks.Get())
    std::move(callback).Run(adapter);

  classic_adapter_callbacks.Get().clear();
}
#endif  // defined(OS_WIN)

}  // namespace

BluetoothAdapterFactory::~BluetoothAdapterFactory() = default;

// static
BluetoothAdapterFactory& BluetoothAdapterFactory::Get() {
  return g_singleton.Get();
}

// static
bool BluetoothAdapterFactory::IsBluetoothSupported() {
  // SetAdapterForTesting() may be used to provide a test or mock adapter
  // instance even on platforms that would otherwise not support it.
  if (default_adapter.Get())
    return true;
#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_LINUX) || \
    defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

bool BluetoothAdapterFactory::IsLowEnergySupported() {
  if (values_for_testing_) {
    return values_for_testing_->GetLESupported();
  }

#if defined(OS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_MARSHMALLOW;
#elif defined(OS_WIN)
  // Windows 8 supports Low Energy GATT operations but it does not support
  // scanning, initiating connections and GATT Server. To keep the API
  // consistent we consider Windows 8 as lacking Low Energy support.
  return base::win::GetVersion() >= base::win::Version::WIN10;
#elif defined(OS_MACOSX)
  return true;
#elif defined(OS_LINUX)
  return true;
#else
  return false;
#endif
}

// static
void BluetoothAdapterFactory::GetAdapter(AdapterCallback callback) {
  DCHECK(IsBluetoothSupported());

#if defined(OS_WIN) || defined(OS_LINUX)
  if (!default_adapter.Get()) {
    default_adapter.Get() =
        BluetoothAdapter::CreateAdapter(base::BindOnce(&RunAdapterCallbacks));
    DCHECK(!default_adapter.Get()->IsInitialized());
  }

  if (!default_adapter.Get()->IsInitialized())
    adapter_callbacks.Get().push_back(std::move(callback));
#else   // !defined(OS_WIN) && !defined(OS_LINUX)
  if (!default_adapter.Get()) {
    default_adapter.Get() =
        BluetoothAdapter::CreateAdapter(base::NullCallback());
  }

  DCHECK(default_adapter.Get()->IsInitialized());
#endif  // defined(OS_WIN) || defined(OS_LINUX)

  if (default_adapter.Get()->IsInitialized()) {
    std::move(callback).Run(
        scoped_refptr<BluetoothAdapter>(default_adapter.Get().get()));
  }
}

// static
void BluetoothAdapterFactory::GetClassicAdapter(AdapterCallback callback) {
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    // Prior to Win10, the default adapter will support Bluetooth classic.
    GetAdapter(std::move(callback));
    return;
  }

  if (!classic_adapter.Get()) {
    classic_adapter.Get() = BluetoothAdapterWin::CreateClassicAdapter(
        base::BindOnce(&RunClassicAdapterCallbacks));
    DCHECK(!classic_adapter.Get()->IsInitialized());
  }

  if (!classic_adapter.Get()->IsInitialized()) {
    classic_adapter_callbacks.Get().push_back(std::move(callback));
  } else {
    std::move(callback).Run(
        scoped_refptr<BluetoothAdapter>(classic_adapter.Get().get()));
  }
#else
  GetAdapter(std::move(callback));
#endif  // defined(OS_WIN)
}

#if defined(OS_LINUX)
// static
void BluetoothAdapterFactory::Shutdown() {
  if (default_adapter.Get())
    default_adapter.Get().get()->Shutdown();
}
#endif

// static
void BluetoothAdapterFactory::SetAdapterForTesting(
    scoped_refptr<BluetoothAdapter> adapter) {
  default_adapter.Get() = adapter->GetWeakPtrForTesting();
#if defined(OS_WIN)
  classic_adapter.Get() = adapter->GetWeakPtrForTesting();
#endif
}

// static
bool BluetoothAdapterFactory::HasSharedInstanceForTesting() {
  return default_adapter.Get() != nullptr;
}

#if defined(OS_CHROMEOS)
// static
void BluetoothAdapterFactory::SetBleScanParserCallback(
    BleScanParserCallback callback) {
  Get().ble_scan_parser_ = callback;
}

// static
BluetoothAdapterFactory::BleScanParserCallback
BluetoothAdapterFactory::GetBleScanParserCallback() {
  return Get().ble_scan_parser_;
}
#endif  // defined(OS_CHROMEOS)

BluetoothAdapterFactory::GlobalValuesForTesting::GlobalValuesForTesting() =
    default;

BluetoothAdapterFactory::GlobalValuesForTesting::~GlobalValuesForTesting() =
    default;

base::WeakPtr<BluetoothAdapterFactory::GlobalValuesForTesting>
BluetoothAdapterFactory::GlobalValuesForTesting::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<BluetoothAdapterFactory::GlobalValuesForTesting>
BluetoothAdapterFactory::InitGlobalValuesForTesting() {
  auto v = std::make_unique<BluetoothAdapterFactory::GlobalValuesForTesting>();
  values_for_testing_ = v->GetWeakPtr();
  return v;
}

BluetoothAdapterFactory::BluetoothAdapterFactory() = default;

}  // namespace device
