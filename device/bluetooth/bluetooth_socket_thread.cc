// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_thread.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"

namespace device {

base::LazyInstance<scoped_refptr<BluetoothSocketThread>>::DestructorAtExit
    g_instance = LAZY_INSTANCE_INITIALIZER;

// static
scoped_refptr<BluetoothSocketThread> BluetoothSocketThread::Get() {
  if (!g_instance.Get().get()) {
    g_instance.Get() = new BluetoothSocketThread();
  }
  return g_instance.Get();
}

// static
void BluetoothSocketThread::CleanupForTesting() {
  DCHECK(g_instance.Get().get());
  g_instance.Get().reset();
}

BluetoothSocketThread::BluetoothSocketThread() = default;

BluetoothSocketThread::~BluetoothSocketThread() {
  if (thread_) {
    thread_->Stop();
    thread_.reset(NULL);
    task_runner_.reset();
  }
}

void BluetoothSocketThread::OnSocketActivate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  active_socket_count_ += 1;
  EnsureStarted();
}

void BluetoothSocketThread::OnSocketDeactivate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  active_socket_count_ -= 1;
}

void BluetoothSocketThread::EnsureStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (thread_)
    return;

  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  thread_ = std::make_unique<base::Thread>("BluetoothSocketThread");
  thread_->StartWithOptions(std::move(thread_options));
  task_runner_ = thread_->task_runner();
}

scoped_refptr<base::SequencedTaskRunner> BluetoothSocketThread::task_runner()
    const {
  DCHECK(active_socket_count_.load(std::memory_order_relaxed) > 0);
  DCHECK(thread_);
  DCHECK(task_runner_.get());

  return task_runner_;
}

}  // namespace device
