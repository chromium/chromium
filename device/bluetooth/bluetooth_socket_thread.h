// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_H_

#include <atomic>
#include <memory>

#include "base/atomicops.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}  // namespace base

namespace device {

// Thread abstraction used by |BluetoothSocketBlueZ| and |BluetoothSocketWin|
// to perform IO operations on the underlying platform sockets. An instance of
// this class can be shared by many active sockets.
class DEVICE_BLUETOOTH_EXPORT BluetoothSocketThread
    : public base::RefCountedThreadSafe<BluetoothSocketThread> {
 public:
  static scoped_refptr<BluetoothSocketThread> Get();

  BluetoothSocketThread(const BluetoothSocketThread&) = delete;
  BluetoothSocketThread& operator=(const BluetoothSocketThread&) = delete;

  static void CleanupForTesting();

  void OnSocketActivate();
  void OnSocketDeactivate();

  scoped_refptr<base::SequencedTaskRunner> task_runner() const;

 private:
  friend class base::RefCountedThreadSafe<BluetoothSocketThread>;
  BluetoothSocketThread();
  virtual ~BluetoothSocketThread();

  void EnsureStarted();

  base::ThreadChecker thread_checker_;
  std::atomic<int> active_socket_count_{0};
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_THREAD_H_
