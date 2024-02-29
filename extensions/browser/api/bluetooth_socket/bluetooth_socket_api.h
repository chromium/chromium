// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "extensions/browser/api/api_resource_manager.h"
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
  raw_ptr<ApiResourceManager<BluetoothApiSocket>> manager_;
};

class BluetoothSocketCreateFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.create", BLUETOOTHSOCKET_CREATE)

  BluetoothSocketCreateFunction();

  BluetoothSocketCreateFunction(const BluetoothSocketCreateFunction&) = delete;
  BluetoothSocketCreateFunction& operator=(
      const BluetoothSocketCreateFunction&) = delete;

 protected:
  ~BluetoothSocketCreateFunction() override;

  ResponseAction Run() override;
};

class BluetoothSocketUpdateFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.update", BLUETOOTHSOCKET_UPDATE)

  BluetoothSocketUpdateFunction();

  BluetoothSocketUpdateFunction(const BluetoothSocketUpdateFunction&) = delete;
  BluetoothSocketUpdateFunction& operator=(
      const BluetoothSocketUpdateFunction&) = delete;

 protected:
  ~BluetoothSocketUpdateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class BluetoothSocketSetPausedFunction
    : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.setPaused",
                             BLUETOOTHSOCKET_SETPAUSED)

  BluetoothSocketSetPausedFunction();

  BluetoothSocketSetPausedFunction(const BluetoothSocketSetPausedFunction&) =
      delete;
  BluetoothSocketSetPausedFunction& operator=(
      const BluetoothSocketSetPausedFunction&) = delete;

 protected:
  ~BluetoothSocketSetPausedFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class BluetoothSocketListenFunction : public BluetoothSocketAsyncApiFunction {
 public:
  BluetoothSocketListenFunction();

  virtual bool CreateParams() = 0;
  virtual void CreateService(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID& uuid,
      const std::optional<std::string>& name,
      device::BluetoothAdapter::CreateServiceCallback callback,
      device::BluetoothAdapter::CreateServiceErrorCallback error_callback) = 0;
  virtual base::Value::List CreateResults() = 0;

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

  raw_ptr<BluetoothSocketEventDispatcher> socket_event_dispatcher_ = nullptr;
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
  void CreateService(scoped_refptr<device::BluetoothAdapter> adapter,
                     const device::BluetoothUUID& uuid,
                     const std::optional<std::string>& name,
                     device::BluetoothAdapter::CreateServiceCallback callback,
                     device::BluetoothAdapter::CreateServiceErrorCallback
                         error_callback) override;
  base::Value::List CreateResults() override;

 protected:
  ~BluetoothSocketListenUsingRfcommFunction() override;

 private:
  std::optional<bluetooth_socket::ListenUsingRfcomm::Params> params_;
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
  void CreateService(scoped_refptr<device::BluetoothAdapter> adapter,
                     const device::BluetoothUUID& uuid,
                     const std::optional<std::string>& name,
                     device::BluetoothAdapter::CreateServiceCallback callback,
                     device::BluetoothAdapter::CreateServiceErrorCallback
                         error_callback) override;
  base::Value::List CreateResults() override;

 protected:
  ~BluetoothSocketListenUsingL2capFunction() override;

 private:
  std::optional<bluetooth_socket::ListenUsingL2cap::Params> params_;
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

  std::optional<bluetooth_socket::Connect::Params> params_;
  raw_ptr<BluetoothSocketEventDispatcher> socket_event_dispatcher_ = nullptr;
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

  BluetoothSocketDisconnectFunction(const BluetoothSocketDisconnectFunction&) =
      delete;
  BluetoothSocketDisconnectFunction& operator=(
      const BluetoothSocketDisconnectFunction&) = delete;

 protected:
  ~BluetoothSocketDisconnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  virtual void OnSuccess();
};

class BluetoothSocketCloseFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.close", BLUETOOTHSOCKET_CLOSE)

  BluetoothSocketCloseFunction();

  BluetoothSocketCloseFunction(const BluetoothSocketCloseFunction&) = delete;
  BluetoothSocketCloseFunction& operator=(const BluetoothSocketCloseFunction&) =
      delete;

 protected:
  ~BluetoothSocketCloseFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class BluetoothSocketSendFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.send", BLUETOOTHSOCKET_SEND)

  BluetoothSocketSendFunction();

  BluetoothSocketSendFunction(const BluetoothSocketSendFunction&) = delete;
  BluetoothSocketSendFunction& operator=(const BluetoothSocketSendFunction&) =
      delete;

 protected:
  ~BluetoothSocketSendFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnSuccess(int bytes_sent);
  void OnError(BluetoothApiSocket::ErrorReason reason,
               const std::string& message);

  std::optional<bluetooth_socket::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class BluetoothSocketGetInfoFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.getInfo", BLUETOOTHSOCKET_GETINFO)

  BluetoothSocketGetInfoFunction();

  BluetoothSocketGetInfoFunction(const BluetoothSocketGetInfoFunction&) =
      delete;
  BluetoothSocketGetInfoFunction& operator=(
      const BluetoothSocketGetInfoFunction&) = delete;

 protected:
  ~BluetoothSocketGetInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
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
