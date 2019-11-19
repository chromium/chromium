// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_thread.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_pump_type.h"
#include "base/sequenced_task_runner.h"
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

BluetoothSocketThread::BluetoothSocketThread()
    : active_socket_count_(0) {}

BluetoothSocketThread::~BluetoothSocketThread() {
  if (thread_) {
    thread_->Stop();
    thread_.reset(NULL);
    task_runner_.reset();
  }
}

void BluetoothSocketThread::OnSocketActivate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  active_socket_count_++;
  EnsureStarted();
}

void BluetoothSocketThread::OnSocketDeactivate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  active_socket_count_--;
}

void BluetoothSocketThread::EnsureStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (thread_)
    return;

  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  thread_.reset(new base::Thread("BluetoothSocketThread"));
  thread_->StartWithOptions(thread_options);
  task_runner_ = thread_->task_runner();
}

scoped_refptr<base::SequencedTaskRunner> BluetoothSocketThread::task_runner()
    const {
  DCHECK(active_socket_count_ > 0);
  DCHECK(thread_);
  DCHECK(task_runner_.get());

  return task_runner_;
}

}  // namespace device
