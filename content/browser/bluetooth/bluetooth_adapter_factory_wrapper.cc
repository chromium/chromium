// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
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
  // Clear adapter.
  set_adapter(nullptr);
}

// static
BluetoothAdapterFactoryWrapper& BluetoothAdapterFactoryWrapper::Get() {
  static base::NoDestructor<BluetoothAdapterFactoryWrapper> singleton;
  return *singleton;
}

bool BluetoothAdapterFactoryWrapper::IsLowEnergySupported() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (adapter_ || test_adapter_)
    return true;
  return BluetoothAdapterFactory::Get()->IsLowEnergySupported();
}

void BluetoothAdapterFactoryWrapper::AcquireAdapter(
    WebBluetoothServiceImpl* service,
    AcquireAdapterCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!GetAdapter(service));

  MaybeAddAdapterObserver(service);
  if (adapter_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), adapter_));
    return;
  }

  // Simulate the normally asynchronous process of acquiring the adapter in
  // tests.
  if (test_adapter_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothAdapterFactoryWrapper::OnGetAdapter,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(callback), test_adapter_));
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
  if (adapter_observers_.empty())
    set_adapter(nullptr);
}

BluetoothAdapter* BluetoothAdapterFactoryWrapper::GetAdapter(
    WebBluetoothServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (HasAdapter(service)) {
    return adapter_.get();
  }
  return nullptr;
}

void BluetoothAdapterFactoryWrapper::SetBluetoothAdapterForTesting(
    scoped_refptr<BluetoothAdapter> test_adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If an adapter has already been acquired allow the adapter to be swapped out
  // synchronously.
  if (adapter_) {
    set_adapter(std::move(test_adapter));
    return;
  }

  test_adapter_ = std::move(test_adapter);
}

void BluetoothAdapterFactoryWrapper::OnGetAdapter(
    AcquireAdapterCallback continuation,
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Clear the adapter configured for testing now that it has been acquired.
  test_adapter_.reset();

  set_adapter(adapter);
  std::move(continuation).Run(adapter_);
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
  if (inserted && adapter_) {
    adapter_->AddObserver(service);
  }
}

void BluetoothAdapterFactoryWrapper::RemoveAdapterObserver(
    WebBluetoothServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  size_t removed = adapter_observers_.erase(service);
  DCHECK(removed);
  if (adapter_) {
    adapter_->RemoveObserver(service);
  }
}

void BluetoothAdapterFactoryWrapper::set_adapter(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // There might be extra unnecessary calls to this method if multiple requests
  // to acquire an adapter were in flight at once.
  if (adapter.get() == adapter_.get())
    return;

  if (adapter_.get()) {
    for (WebBluetoothServiceImpl* service : adapter_observers_) {
      adapter_->RemoveObserver(service);
    }
  }
  adapter_ = std::move(adapter);
  if (adapter_.get()) {
    for (WebBluetoothServiceImpl* service : adapter_observers_) {
      adapter_->AddObserver(service);
    }
  }
}

}  // namespace content
