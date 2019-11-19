// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_set>

#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/common/api/bluetooth_socket.h"

namespace device {
class BluetoothSocket;
}

namespace net {
class IOBuffer;
}

namespace extensions {

namespace api {

class BluetoothSocketEventDispatcher;

// Asynchronous API function that performs its work on the BluetoothApiSocket
// thread while providing methods to manage resources of that class. This
// follows the pattern of AsyncApiFunction, but does not derive from it,
// because BluetoothApiSocket methods must be called on the UI Thread.
class BluetoothSocketAsyncApiFunction : public ExtensionFunction {
 public:
  BluetoothSocketAsyncApiFunction();

 protected:
  ~BluetoothSocketAsyncApiFunction() override;

  // ExtensionFunction:
  bool PreRunValidation(std::string* error) override;

  content::BrowserThread::ID work_thread_id() const;

  int AddSocket(BluetoothApiSocket* socket);
  BluetoothApiSocket* GetSocket(int api_resource_id);
  void RemoveSocket(int api_resource_id);
  std::unordered_set<int>* GetSocketIds();

 private:
  ApiResourceManager<BluetoothApiSocket>* manager_;
};

class BluetoothSocketCreateFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.create", BLUETOOTHSOCKET_CREATE)

  BluetoothSocketCreateFunction();

 protected:
  ~BluetoothSocketCreateFunction() override;

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketCreateFunction);
};

class BluetoothSocketUpdateFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.update", BLUETOOTHSOCKET_UPDATE)

  BluetoothSocketUpdateFunction();

 protected:
  ~BluetoothSocketUpdateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketUpdateFunction);
};

class BluetoothSocketSetPausedFunction
    : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.setPaused",
                             BLUETOOTHSOCKET_SETPAUSED)

  BluetoothSocketSetPausedFunction();

 protected:
  ~BluetoothSocketSetPausedFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketSetPausedFunction);
};

class BluetoothSocketListenFunction : public BluetoothSocketAsyncApiFunction {
 public:
  BluetoothSocketListenFunction();

  virtual bool CreateParams() = 0;
  virtual void CreateService(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID& uuid,
      std::unique_ptr<std::string> name,
      const device::BluetoothAdapter::CreateServiceCallback& callback,
      const device::BluetoothAdapter::CreateServiceErrorCallback&
          error_callback) = 0;
  virtual std::unique_ptr<base::ListValue> CreateResults() = 0;

  virtual int socket_id() const = 0;
  virtual const std::string& uuid() const = 0;

  // ExtensionFunction:
  ResponseAction Run() override;
  bool PreRunValidation(std::string* error) override;

 protected:
  ~BluetoothSocketListenFunction() override;

  virtual void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  virtual void OnCreateService(scoped_refptr<device::BluetoothSocket> socket);
  virtual void OnCreateServiceError(const std::string& message);

  BluetoothSocketEventDispatcher* socket_event_dispatcher_ = nullptr;
};

class BluetoothSocketListenUsingRfcommFunction
    : public BluetoothSocketListenFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.listenUsingRfcomm",
                             BLUETOOTHSOCKET_LISTENUSINGRFCOMM)

  BluetoothSocketListenUsingRfcommFunction();

  // BluetoothSocketListenFunction:
  int socket_id() const override;
  const std::string& uuid() const override;

  bool CreateParams() override;
  void CreateService(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID& uuid,
      std::unique_ptr<std::string> name,
      const device::BluetoothAdapter::CreateServiceCallback& callback,
      const device::BluetoothAdapter::CreateServiceErrorCallback&
          error_callback) override;
  std::unique_ptr<base::ListValue> CreateResults() override;

 protected:
  ~BluetoothSocketListenUsingRfcommFunction() override;

 private:
  std::unique_ptr<bluetooth_socket::ListenUsingRfcomm::Params> params_;
};

class BluetoothSocketListenUsingL2capFunction
    : public BluetoothSocketListenFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.listenUsingL2cap",
                             BLUETOOTHSOCKET_LISTENUSINGL2CAP)

  BluetoothSocketListenUsingL2capFunction();

  // BluetoothSocketListenFunction:
  int socket_id() const override;
  const std::string& uuid() const override;

  bool CreateParams() override;
  void CreateService(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID& uuid,
      std::unique_ptr<std::string> name,
      const device::BluetoothAdapter::CreateServiceCallback& callback,
      const device::BluetoothAdapter::CreateServiceErrorCallback&
          error_callback) override;
  std::unique_ptr<base::ListValue> CreateResults() override;

 protected:
  ~BluetoothSocketListenUsingL2capFunction() override;

 private:
  std::unique_ptr<bluetooth_socket::ListenUsingL2cap::Params> params_;
};

class BluetoothSocketAbstractConnectFunction :
    public BluetoothSocketAsyncApiFunction {
 public:
  BluetoothSocketAbstractConnectFunction();

 protected:
  ~BluetoothSocketAbstractConnectFunction() override;

  // ExtensionFunction:
  bool PreRunValidation(std::string* error) override;
  ResponseAction Run() override;

  // Subclasses should implement this method to connect to the service
  // registered with |uuid| on the |device|.
  virtual void ConnectToService(device::BluetoothDevice* device,
                                const device::BluetoothUUID& uuid) = 0;

  virtual void OnConnect(scoped_refptr<device::BluetoothSocket> socket);
  virtual void OnConnectError(const std::string& message);

 private:
  virtual void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  std::unique_ptr<bluetooth_socket::Connect::Params> params_;
  BluetoothSocketEventDispatcher* socket_event_dispatcher_ = nullptr;
};

class BluetoothSocketConnectFunction :
    public BluetoothSocketAbstractConnectFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.connect", BLUETOOTHSOCKET_CONNECT)

  BluetoothSocketConnectFunction();

 protected:
  ~BluetoothSocketConnectFunction() override;

  // BluetoothSocketAbstractConnectFunction:
  void ConnectToService(device::BluetoothDevice* device,
                        const device::BluetoothUUID& uuid) override;
};

class BluetoothSocketDisconnectFunction
    : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.disconnect",
                             BLUETOOTHSOCKET_DISCONNECT)

  BluetoothSocketDisconnectFunction();

 protected:
  ~BluetoothSocketDisconnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  virtual void OnSuccess();

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketDisconnectFunction);
};

class BluetoothSocketCloseFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.close", BLUETOOTHSOCKET_CLOSE)

  BluetoothSocketCloseFunction();

 protected:
  ~BluetoothSocketCloseFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketCloseFunction);
};

class BluetoothSocketSendFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.send", BLUETOOTHSOCKET_SEND)

  BluetoothSocketSendFunction();

 protected:
  ~BluetoothSocketSendFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnSuccess(int bytes_sent);
  void OnError(BluetoothApiSocket::ErrorReason reason,
               const std::string& message);

  std::unique_ptr<bluetooth_socket::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketSendFunction);
};

class BluetoothSocketGetInfoFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.getInfo", BLUETOOTHSOCKET_GETINFO)

  BluetoothSocketGetInfoFunction();

 protected:
  ~BluetoothSocketGetInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketGetInfoFunction);
};

class BluetoothSocketGetSocketsFunction
    : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.getSockets",
                             BLUETOOTHSOCKET_GETSOCKETS)

  BluetoothSocketGetSocketsFunction();

 protected:
  ~BluetoothSocketGetSocketsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
