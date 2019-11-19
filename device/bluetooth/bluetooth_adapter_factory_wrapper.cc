// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

static base::LazyInstance<device::BluetoothAdapterFactoryWrapper>::Leaky
    g_bluetooth_adapter_factory_wrapper_singleton = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace device {

BluetoothAdapterFactoryWrapper::~BluetoothAdapterFactoryWrapper() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // All observers should have been removed already.
  DCHECK(adapter_observers_.empty());
  // Clear adapter.
  set_adapter(scoped_refptr<BluetoothAdapter>());
}

// static
BluetoothAdapterFactoryWrapper& BluetoothAdapterFactoryWrapper::Get() {
  return g_bluetooth_adapter_factory_wrapper_singleton.Get();
}

bool BluetoothAdapterFactoryWrapper::IsLowEnergySupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (adapter_ != nullptr) {
    return true;
  }
  return BluetoothAdapterFactory::Get().IsLowEnergySupported();
}

void BluetoothAdapterFactoryWrapper::AcquireAdapter(
    BluetoothAdapter::Observer* observer,
    AcquireAdapterCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!GetAdapter(observer));

  AddAdapterObserver(observer);
  if (adapter_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), adapter_));
    return;
  }

  DCHECK(BluetoothAdapterFactory::Get().IsLowEnergySupported());
  BluetoothAdapterFactory::GetAdapter(
      base::BindOnce(&BluetoothAdapterFactoryWrapper::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothAdapterFactoryWrapper::ReleaseAdapter(
    BluetoothAdapter::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!HasAdapter(observer)) {
    return;
  }
  RemoveAdapterObserver(observer);
  if (adapter_observers_.empty())
    set_adapter(scoped_refptr<BluetoothAdapter>());
}

BluetoothAdapter* BluetoothAdapterFactoryWrapper::GetAdapter(
    BluetoothAdapter::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (HasAdapter(observer)) {
    return adapter_.get();
  }
  return nullptr;
}

void BluetoothAdapterFactoryWrapper::SetBluetoothAdapterForTesting(
    scoped_refptr<BluetoothAdapter> mock_adapter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  set_adapter(std::move(mock_adapter));
}

BluetoothAdapterFactoryWrapper::BluetoothAdapterFactoryWrapper() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BluetoothAdapterFactoryWrapper::OnGetAdapter(
    AcquireAdapterCallback continuation,
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(thread_checker_.CalledOnValidThread());

  set_adapter(adapter);
  std::move(continuation).Run(adapter_);
}

bool BluetoothAdapterFactoryWrapper::HasAdapter(
    BluetoothAdapter::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return base::Contains(adapter_observers_, observer);
}

void BluetoothAdapterFactoryWrapper::AddAdapterObserver(
    BluetoothAdapter::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto iter = adapter_observers_.insert(observer);
  DCHECK(iter.second);
  if (adapter_) {
    adapter_->AddObserver(observer);
  }
}

void BluetoothAdapterFactoryWrapper::RemoveAdapterObserver(
    BluetoothAdapter::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  size_t removed = adapter_observers_.erase(observer);
  DCHECK(removed);
  if (adapter_) {
    adapter_->RemoveObserver(observer);
  }
}

void BluetoothAdapterFactoryWrapper::set_adapter(
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (adapter_.get()) {
    for (BluetoothAdapter::Observer* observer : adapter_observers_) {
      adapter_->RemoveObserver(observer);
    }
  }
  adapter_ = adapter;
  if (adapter_.get()) {
    for (BluetoothAdapter::Observer* observer : adapter_observers_) {
      adapter_->AddObserver(observer);
    }
  }
}

}  // namespace device
