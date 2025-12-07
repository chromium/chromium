// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "net/base/io_buffer.h"

namespace device {

class BluetoothSocketThread;
class Outcome;

class DEVICE_BLUETOOTH_EXPORT BluetoothSocketAndroid : public BluetoothSocket {
 public:
  static scoped_refptr<BluetoothSocketAndroid> Create(
      base::android::ScopedJavaLocalRef<jobject>
          socket_wrapper,  // Java Type: BluetoothSocketWrapper
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<BluetoothSocketThread> socket_thread);

  BluetoothSocketAndroid(base::android::ScopedJavaLocalRef<jobject> j_socket,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         scoped_refptr<BluetoothSocketThread> socket_thread);

  BluetoothSocketAndroid(const BluetoothSocketAndroid&) = delete;
  BluetoothSocketAndroid& operator=(const BluetoothSocketAndroid&) = delete;

  // Returns the associated ChromeBluetoothSocket Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

  // Connects this socket to the bound service.  On a successful connection,
  // the socket properties will be updated and |success_callback| called. On
  // failure, |error_callback| will be called with a message explaining the
  // cause of failure.
  void Connect(base::OnceClosure success_callback,
               ErrorCompletionCallback error_callback);

  // BluetoothSocket:
  void Disconnect(base::OnceClosure success_callback) override;
  void Receive(int buffer_size,
               ReceiveCompletionCallback success_callback,
               ReceiveErrorCompletionCallback error_callback) override;
  void Send(scoped_refptr<net::IOBuffer> buffer,
            int buffer_size,
            SendCompletionCallback success_callback,
            ErrorCompletionCallback error_callback) override;
  void Accept(AcceptCompletionCallback success_callback,
              ErrorCompletionCallback error_callback) override;

 private:
  ~BluetoothSocketAndroid() override;

  void DispatchErrorCallback(ErrorCompletionCallback error_callback,
                             const Outcome& outcome);  // Java Type: Outcome

  void DoConnect(base::OnceClosure success_callback,
                 ErrorCompletionCallback error_callback);
  void DoDisconnect(base::OnceClosure success_callback);
  void DoReceive(size_t buffer_size,
                 ReceiveCompletionCallback success_callback,
                 ReceiveErrorCompletionCallback error_callback);
  void DoSend(scoped_refptr<net::IOBuffer> buffer,
              size_t buffer_size,
              SendCompletionCallback success_callback,
              ErrorCompletionCallback error_callback);

  // UI thread task runner.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // Socket thread used to perform blocking IO operations except receiving.
  scoped_refptr<BluetoothSocketThread> socket_thread_;

  // A dedicated thread for receiving data. Android Bluetooth socket uses Java's
  // InputStream which requires a dedicated thread to listen to data.
  std::unique_ptr<base::Thread> receiving_thread_;

  // Java object org.chromium.device.bluetooth.ChromeBluetoothSocket.
  base::android::ScopedJavaGlobalRef<jobject> j_socket_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_ANDROID_H_
