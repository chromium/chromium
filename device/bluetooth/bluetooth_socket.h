// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_export.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace device {

class BluetoothDevice;

// BluetoothSocket represents a socket to a specific service on a
// BluetoothDevice.  BluetoothSocket objects are ref counted and may outlive
// both the BluetoothDevice and BluetoothAdapter that were involved in their
// creation.  In terms of threading, platform specific implementations may
// differ slightly, but platform independent consumers must guarantee calling
// various instance methods on the same thread as the thread used at
// construction time -- platform specific implementation are responsible for
// marshalling calls to a different thread if required.
class DEVICE_BLUETOOTH_EXPORT BluetoothSocket
    : public base::RefCountedThreadSafe<BluetoothSocket> {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This enum should be kept in sync
  // with the BluetoothSocketErrorReason enum in
  // src/tools/metrics/histograms/enums.xml.
  enum ErrorReason {
    kSystemError = 0,
    kIOPending = 1,
    kDisconnected = 2,
    kMaxValue = kDisconnected,
  };

  using SendCompletionCallback = base::OnceCallback<void(int)>;
  using ReceiveCompletionCallback =
      base::OnceCallback<void(int, scoped_refptr<net::IOBuffer> io_buffer)>;
  using AcceptCompletionCallback =
      base::OnceCallback<void(const BluetoothDevice* device,
                              scoped_refptr<BluetoothSocket> socket)>;
  using ErrorCompletionCallback =
      base::OnceCallback<void(const std::string& error_message)>;
  using ReceiveErrorCompletionCallback =
      base::OnceCallback<void(ErrorReason, const std::string& error_message)>;

  // Gracefully disconnects the socket and calls |callback| upon completion.
  // After calling this method, it is illegal to call any method on this socket
  // instance (except for the destructor via Release).
  // There is no failure case, as this is a best effort operation.
  virtual void Disconnect(base::OnceClosure success_callback) = 0;

  // Receives data from the socket and calls |success_callback| when data is
  // available. |buffer_size| specifies the maximum number of bytes that can be
  // received. If an error occurs, calls |error_callback| with a reason and an
  // error message.
  virtual void Receive(int buffer_size,
                       ReceiveCompletionCallback success_callback,
                       ReceiveErrorCompletionCallback error_callback) = 0;

  // Sends |buffer| to the socket and calls |success_callback| when data has
  // been successfully sent. |buffer_size| is the number of bytes contained in
  // |buffer|. If an error occurs, calls |error_callback| with an error message.
  virtual void Send(scoped_refptr<net::IOBuffer> buffer,
                    int buffer_size,
                    SendCompletionCallback success_callback,
                    ErrorCompletionCallback error_callback) = 0;

  // Accepts a pending client connection from the socket and calls
  // |success_callback| on completion, passing a new BluetoothSocket instance
  // for the new client. If an error occurs, calls |error_callback| with a
  // reason and an error message.
  virtual void Accept(AcceptCompletionCallback success_callback,
                      ErrorCompletionCallback error_callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BluetoothSocket>;
  virtual ~BluetoothSocket();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_H_
