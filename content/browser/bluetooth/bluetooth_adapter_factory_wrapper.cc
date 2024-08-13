// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"

#include <stddef.h>

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace content {

namespace {
using ::device::BluetoothAdapter;
using ::device::BluetoothAdapterFactory;
}  // namespace

BluetoothAdapterFactoryWrapper::BluetoothAdapterFactoryWrapper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

BluetoothAdapterFactoryWrapper::~BluetoothAdapterFactoryWrapper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // All observers should have been removed already.
  DCHECK(adapter_observers_.empty());
  // Clear adapters.
  SetAdapterInternal(/*adapter=*/nullptr, /*is_override_adapter=*/true);
  SetAdapterInternal(/*adapter=*/nullptr, /*is_override_adapter=*/false);
}

// static
BluetoothAdapterFactoryWrapper& BluetoothAdapterFactoryWrapper::Get() {
  static base::NoDestructor<BluetoothAdapterFactoryWrapper> singleton;
  return *singleton;
}

bool BluetoothAdapterFactoryWrapper::IsLowEnergySupported() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (GetActiveAdapter() || pending_override_adapter_) {
    return true;
  }
  return BluetoothAdapterFactory::Get()->IsLowEnergySupported();
}

void BluetoothAdapterFactoryWrapper::AcquireAdapter(
    WebBluetoothServiceImpl* service,
    AcquireAdapterCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!GetAdapter(service));

  MaybeAddAdapterObserver(service);
  scoped_refptr<BluetoothAdapter> active_adapter = GetActiveAdapter();
  if (active_adapter) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(active_adapter)));
    return;
  }

  // Simulate the normally asynchronous process of acquiring the adapter in
  // tests.
  if (pending_override_adapter_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&BluetoothAdapterFactoryWrapper::OnGetOverrideAdapter,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       pending_override_adapter_));
    return;
  }

  DCHECK(BluetoothAdapterFactory::Get()->IsLowEnergySupported());
  BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BluetoothAdapterFactoryWrapper::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothAdapterFactoryWrapper::ReleaseAdapter(
    WebBluetoothServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!HasAdapter(service)) {
    return;
  }
  RemoveAdapterObserver(service);
  if (adapter_observers_.empty()) {
    SetAdapterInternal(/*adapter=*/nullptr, /*is_override_adapter=*/true);
    SetAdapterInternal(/*adapter=*/nullptr, /*is_override_adapter=*/false);
  }
}

BluetoothAdapter* BluetoothAdapterFactoryWrapper::GetAdapter(
    WebBluetoothServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (HasAdapter(service)) {
    return GetActiveAdapter().get();
  }
  return nullptr;
}

scoped_refptr<BluetoothAdapter>
BluetoothAdapterFactoryWrapper::GetActiveAdapter() {
  if (override_adapter_) {
    return override_adapter_;
  }
  if (adapter_) {
    return adapter_;
  }
  return nullptr;
}

void BluetoothAdapterFactoryWrapper::SetBluetoothAdapterOverride(
    scoped_refptr<BluetoothAdapter> override_adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If an adapter has already been acquired allow the adapter to be swapped out
  // synchronously.
  if (GetActiveAdapter()) {
    SetAdapterInternal(std::move(override_adapter),
                       /*is_override_adapter=*/true);
    return;
  }

  pending_override_adapter_ = std::move(override_adapter);
}

void BluetoothAdapterFactoryWrapper::OnGetAdapter(
    AcquireAdapterCallback continuation,
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SetAdapterInternal(adapter, /*is_override_adapter=*/false);
  std::move(continuation).Run(GetActiveAdapter());
}

void BluetoothAdapterFactoryWrapper::OnGetOverrideAdapter(
    AcquireAdapterCallback continuation,
    scoped_refptr<BluetoothAdapter> override_adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Clear the adapter configured for override now that it has been acquired.
  pending_override_adapter_.reset();

  SetAdapterInternal(override_adapter, /*is_override_adapter=*/true);
  std::move(continuation).Run(GetActiveAdapter());
}

bool BluetoothAdapterFactoryWrapper::HasAdapter(
    WebBluetoothServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return base::Contains(adapter_observers_, service);
}

void BluetoothAdapterFactoryWrapper::MaybeAddAdapterObserver(
    WebBluetoothServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // The same WebBluetoothServiceImpl might acquire the adapter multiple times
  // if it gets multiple requests in parallel before the adapter is ready but is
  // guaranteed to only call ReleaseAdapter() once on destruction.
  auto [it, inserted] = adapter_observers_.insert(service);
  scoped_refptr<BluetoothAdapter> active_adapter = GetActiveAdapter();
  if (inserted && active_adapter) {
    active_adapter->AddObserver(service);
  }
}

void BluetoothAdapterFactoryWrapper::RemoveAdapterObserver(
    WebBluetoothServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  size_t removed = adapter_observers_.erase(service);
  DCHECK(removed);
  scoped_refptr<BluetoothAdapter> active_adapter = GetActiveAdapter();
  if (active_adapter) {
    active_adapter->RemoveObserver(service);
  }
}

void BluetoothAdapterFactoryWrapper::SetAdapterInternal(
    scoped_refptr<BluetoothAdapter> adapter,
    bool is_override_adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // There might be extra unnecessary calls to this method if multiple requests
  // to acquire an adapter were in flight at once.
  scoped_refptr<BluetoothAdapter> active_adapter = GetActiveAdapter();
  if (adapter == active_adapter) {
    return;
  }

  if (active_adapter) {
    for (WebBluetoothServiceImpl* service : adapter_observers_) {
      active_adapter->RemoveObserver(service);
    }
  }

  if (is_override_adapter) {
    override_adapter_ = std::move(adapter);
  } else {
    adapter_ = std::move(adapter);
  }

  // Update active adapter as it might have changed.
  active_adapter = GetActiveAdapter();
  if (active_adapter) {
    for (WebBluetoothServiceImpl* service : adapter_observers_) {
      active_adapter->AddObserver(service);
    }
  }
}

}  // namespace content
