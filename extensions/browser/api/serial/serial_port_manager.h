// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SERIAL_SERIAL_PORT_MANAGER_H_
#define EXTENSIONS_BROWSER_API_SERIAL_SERIAL_PORT_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/api/serial.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace content {
class BrowserContext;
}

namespace extensions {

struct Event;
class SerialConnection;

namespace api {
// Per-browser-context dispatcher for events on serial connections.
class SerialPortManager : public BrowserContextKeyedAPI {
 public:
  using OpenPortCallback =
      base::OnceCallback<void(mojo::PendingRemote<device::mojom::SerialPort>)>;

  static SerialPortManager* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SerialPortManager>* GetFactoryInstance();

  explicit SerialPortManager(content::BrowserContext* context);

  SerialPortManager(const SerialPortManager&) = delete;
  SerialPortManager& operator=(const SerialPortManager&) = delete;

  ~SerialPortManager() override;

  void GetDevices(
      device::mojom::SerialPortManager::GetDevicesCallback callback);
  void OpenPort(const std::string& path,
                device::mojom::SerialConnectionOptionsPtr options,
                mojo::PendingRemote<device::mojom::SerialPortClient> client,
                OpenPortCallback callback);

  // Start the poilling process for the connection.
  void StartConnectionPolling(const ExtensionId& extension_id,
                              int connection_id);

  // Allows tests to override how this class binds SerialPortManager receivers.
  using Binder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::SerialPortManager>)>;
  static void OverrideBinderForTesting(Binder binder);

 private:
  using ConnectionData = ApiResourceManager<SerialConnection>::ApiResourceData;
  friend class BrowserContextKeyedAPIFactory<SerialPortManager>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SerialPortManager"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  struct ReceiveParams {
    ReceiveParams();
    ReceiveParams(const ReceiveParams& other);
    ~ReceiveParams();

    raw_ptr<void> browser_context_id;
    ExtensionId extension_id;
    scoped_refptr<ConnectionData> connections;
    int connection_id;
  };
  static void DispatchReceiveEvent(const ReceiveParams& params,
                                   std::vector<uint8_t> data,
                                   serial::ReceiveError error);

  static void DispatchEvent(const ReceiveParams& params,
                            std::unique_ptr<extensions::Event> event);

  void EnsureConnection();
  void OnGotDevicesToGetPort(
      const std::string& path,
      device::mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<device::mojom::SerialPortClient> client,
      OpenPortCallback callback,
      std::vector<device::mojom::SerialPortInfoPtr> devices);
  void OnPortManagerConnectionError();

  mojo::Remote<device::mojom::SerialPortManager> port_manager_;
  content::BrowserThread::ID thread_id_;
  scoped_refptr<ConnectionData> connections_;
  const raw_ptr<content::BrowserContext> context_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<SerialPortManager> weak_factory_{this};
};

}  // namespace api

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SERIAL_SERIAL_PORT_MANAGER_H_
