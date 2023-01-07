// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SERIAL_SERIAL_API_H_
#define EXTENSIONS_BROWSER_API_SERIAL_SERIAL_API_H_

#include <memory>
#include <string>
#include <vector>

#include "extensions/browser/extension_function.h"
#include "extensions/common/api/serial.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace extensions {

class SerialConnection;

namespace api {

class SerialExtensionFunction : public ExtensionFunction {
 public:
  SerialExtensionFunction();

 protected:
  ~SerialExtensionFunction() override;

  SerialConnection* GetSerialConnection(int api_resource_id);
  void RemoveSerialConnection(int api_resource_id);
};

class SerialGetDevicesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getDevices", SERIAL_GETDEVICES)

  SerialGetDevicesFunction();

  SerialGetDevicesFunction(const SerialGetDevicesFunction&) = delete;
  SerialGetDevicesFunction& operator=(const SerialGetDevicesFunction&) = delete;

 protected:
  ~SerialGetDevicesFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnGotDevices(std::vector<device::mojom::SerialPortInfoPtr> devices);
};

class SerialConnectFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.connect", SERIAL_CONNECT)

  SerialConnectFunction();

 protected:
  ~SerialConnectFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnConnected(bool success);
  void FinishConnect(bool connected,
                     bool got_complete_info,
                     std::unique_ptr<serial::ConnectionInfo> info);

  // This connection is created within SerialConnectFunction.
  // From there its ownership is transferred to the
  // ApiResourceManager<SerialConnection> upon success.
  std::unique_ptr<SerialConnection> connection_;
};

class SerialUpdateFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.update", SERIAL_UPDATE)

  SerialUpdateFunction();

 protected:
  ~SerialUpdateFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnUpdated(bool success);
};

class SerialDisconnectFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.disconnect", SERIAL_DISCONNECT)

  SerialDisconnectFunction();

 protected:
  ~SerialDisconnectFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnCloseComplete(int connection_id);
};

class SerialSetPausedFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.setPaused", SERIAL_SETPAUSED)

  SerialSetPausedFunction();

 protected:
  ~SerialSetPausedFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;
};

class SerialGetInfoFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getInfo", SERIAL_GETINFO)

  SerialGetInfoFunction();

 protected:
  ~SerialGetInfoFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnGotInfo(int connection_id,
                 bool got_complete_info,
                 std::unique_ptr<serial::ConnectionInfo> info);
};

class SerialGetConnectionsFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getConnections", SERIAL_GETCONNECTIONS)

  SerialGetConnectionsFunction();

 protected:
  ~SerialGetConnectionsFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnGotOne(int connection_id,
                bool got_complete_info,
                std::unique_ptr<serial::ConnectionInfo> info);
  void OnGotAll();

  size_t count_ = 0;
  std::vector<serial::ConnectionInfo> infos_;
};

class SerialSendFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.send", SERIAL_SEND)

  SerialSendFunction();

 protected:
  ~SerialSendFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnSendComplete(uint32_t bytes_sent, serial::SendError error);
};

class SerialFlushFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.flush", SERIAL_FLUSH)

  SerialFlushFunction();

 protected:
  ~SerialFlushFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnFlushed();
};

class SerialGetControlSignalsFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getControlSignals",
                             SERIAL_GETCONTROLSIGNALS)

  SerialGetControlSignalsFunction();

 protected:
  ~SerialGetControlSignalsFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnGotControlSignals(
      std::unique_ptr<serial::DeviceControlSignals> signals);
};

class SerialSetControlSignalsFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.setControlSignals",
                             SERIAL_SETCONTROLSIGNALS)

  SerialSetControlSignalsFunction();

 protected:
  ~SerialSetControlSignalsFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnSetControlSignals(bool success);
};

class SerialSetBreakFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.setBreak", SERIAL_SETBREAK)
  SerialSetBreakFunction();

 protected:
  ~SerialSetBreakFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnSetBreak(bool success);
};

class SerialClearBreakFunction : public SerialExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.clearBreak", SERIAL_CLEARBREAK)
  SerialClearBreakFunction();

 protected:
  ~SerialClearBreakFunction() override;

  // ExtensionFunction
  ResponseAction Run() override;

 private:
  void OnClearBreak(bool success);
};

}  // namespace api

}  // namespace extensions

namespace mojo {

template <>
struct TypeConverter<extensions::api::serial::DeviceInfo,
                     device::mojom::SerialPortInfoPtr> {
  static extensions::api::serial::DeviceInfo Convert(
      const device::mojom::SerialPortInfoPtr& input);
};

}  // namespace mojo

#endif  // EXTENSIONS_BROWSER_API_SERIAL_SERIAL_API_H_
