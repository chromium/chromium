// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter.h"

#if defined(OS_MAC)
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

BluetoothAdapterFactory::BluetoothAdapterFactory() = default;

BluetoothAdapterFactory::~BluetoothAdapterFactory() = default;

// static
BluetoothAdapterFactory* BluetoothAdapterFactory::Get() {
  static base::NoDestructor<BluetoothAdapterFactory> factory;
  return factory.get();
}

// static
bool BluetoothAdapterFactory::IsBluetoothSupported() {
  // SetAdapterForTesting() may be used to provide a test or mock adapter
  // instance even on platforms that would otherwise not support it.
  if (Get()->adapter_)
    return true;
#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS) || defined(OS_MAC)
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
#elif defined(OS_MAC)
  return true;
#elif (defined(OS_LINUX) || defined(OS_CHROMEOS))
  return true;
#else
  return false;
#endif
}

void BluetoothAdapterFactory::GetAdapter(AdapterCallback callback) {
  DCHECK(IsBluetoothSupported());

  if (!adapter_) {
    adapter_callbacks_.push_back(std::move(callback));

    adapter_under_initialization_ = BluetoothAdapter::CreateAdapter();
    adapter_ = adapter_under_initialization_->GetWeakPtr();
    adapter_->Initialize(base::BindOnce(
        &BluetoothAdapterFactory::AdapterInitialized, base::Unretained(this)));
    return;
  }

  if (!adapter_->IsInitialized()) {
    adapter_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run(scoped_refptr<BluetoothAdapter>(adapter_.get()));
}

void BluetoothAdapterFactory::GetClassicAdapter(AdapterCallback callback) {
#if defined(OS_WIN)
  DCHECK(IsBluetoothSupported());

  if (base::win::GetVersion() < base::win::Version::WIN10) {
    // Prior to Win10, the default adapter will support Bluetooth classic.
    GetAdapter(std::move(callback));
    return;
  }

  if (!classic_adapter_) {
    classic_adapter_callbacks_.push_back(std::move(callback));

    classic_adapter_under_initialization_ =
        BluetoothAdapterWin::CreateClassicAdapter();
    classic_adapter_ = classic_adapter_under_initialization_->GetWeakPtr();
    classic_adapter_->Initialize(
        base::BindOnce(&BluetoothAdapterFactory::ClassicAdapterInitialized,
                       base::Unretained(this)));
    return;
  }

  if (!classic_adapter_->IsInitialized()) {
    classic_adapter_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run(
      scoped_refptr<BluetoothAdapter>(classic_adapter_.get()));
#else
  GetAdapter(std::move(callback));
#endif  // defined(OS_WIN)
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// static
void BluetoothAdapterFactory::Shutdown() {
  if (Get()->adapter_)
    Get()->adapter_->Shutdown();
}
#endif

// static
void BluetoothAdapterFactory::SetAdapterForTesting(
    scoped_refptr<BluetoothAdapter> adapter) {
  Get()->adapter_ = adapter->GetWeakPtrForTesting();
#if defined(OS_WIN)
  Get()->classic_adapter_ = adapter->GetWeakPtrForTesting();
#endif
}

// static
bool BluetoothAdapterFactory::HasSharedInstanceForTesting() {
  return Get()->adapter_ != nullptr;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
void BluetoothAdapterFactory::SetBleScanParserCallback(
    BleScanParserCallback callback) {
  Get()->ble_scan_parser_ = callback;
}

// static
BluetoothAdapterFactory::BleScanParserCallback
BluetoothAdapterFactory::GetBleScanParserCallback() {
  return Get()->ble_scan_parser_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

void BluetoothAdapterFactory::AdapterInitialized() {
  DCHECK(adapter_);
  DCHECK(adapter_under_initialization_);

  // Move |adapter_under_initialization_| and |adapter_callbacks_| to avoid
  // potential re-entrancy issues while looping over the callbacks.
  scoped_refptr<BluetoothAdapter> adapter =
      std::move(adapter_under_initialization_);
  std::vector<AdapterCallback> callbacks = std::move(adapter_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(adapter);
}

#if defined(OS_WIN)
void BluetoothAdapterFactory::ClassicAdapterInitialized() {
  DCHECK(classic_adapter_);
  DCHECK(classic_adapter_under_initialization_);

  // Move |adapter_under_initialization_| and |adapter_callbacks_| to avoid
  // potential re-entrancy issues while looping over the callbacks.
  scoped_refptr<BluetoothAdapter> adapter =
      std::move(classic_adapter_under_initialization_);
  std::vector<AdapterCallback> callbacks =
      std::move(classic_adapter_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(adapter);
}
#endif  // defined(OS_WIN)

}  // namespace device
