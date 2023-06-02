// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERIAL_SERIAL_SERVICE_H_
#define CONTENT_BROWSER_SERIAL_SERIAL_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/serial_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

namespace content {

class RenderFrameHost;
class SerialChooser;

class SerialService : public blink::mojom::SerialService,
                      public SerialDelegate::Observer,
                      public device::mojom::SerialPortConnectionWatcher,
                      public content::DocumentUserData<SerialService> {
 public:
  explicit SerialService(RenderFrameHost* render_frame_host);

  SerialService(const SerialService&) = delete;
  SerialService& operator=(const SerialService&) = delete;

  ~SerialService() override;

  void Bind(mojo::PendingReceiver<blink::mojom::SerialService> receiver);

  // SerialService implementation
  void SetClient(
      mojo::PendingRemote<blink::mojom::SerialServiceClient> client) override;
  void GetPorts(GetPortsCallback callback) override;
  void RequestPort(std::vector<blink::mojom::SerialPortFilterPtr> filters,
                   const std::vector<::device::BluetoothUUID>&
                       allowed_bluetooth_service_class_ids,
                   RequestPortCallback callback) override;
  void OpenPort(const base::UnguessableToken& token,
                device::mojom::SerialConnectionOptionsPtr options,
                mojo::PendingRemote<device::mojom::SerialPortClient> client,
                OpenPortCallback callback) override;
  void ForgetPort(const base::UnguessableToken& token,
                  ForgetPortCallback callback) override;

  // SerialDelegate::Observer implementation
  void OnPortAdded(const device::mojom::SerialPortInfo& port) override;
  void OnPortRemoved(const device::mojom::SerialPortInfo& port) override;
  void OnPortManagerConnectionError() override;
  void OnPermissionRevoked(const url::Origin& origin) override;

 private:
  friend class content::DocumentUserData<SerialService>;

  void FinishGetPorts(GetPortsCallback callback,
                      std::vector<device::mojom::SerialPortInfoPtr> ports);
  void FinishRequestPort(RequestPortCallback callback,
                         device::mojom::SerialPortInfoPtr port);
  void OnWatcherConnectionError();
  void DecrementActiveFrameCount();

  mojo::ReceiverSet<blink::mojom::SerialService> receivers_;
  mojo::RemoteSet<blink::mojom::SerialServiceClient> clients_;

  // The last shown serial port chooser UI.
  std::unique_ptr<SerialChooser> chooser_;

  // Each pipe here watches a connection created by GetPort() in order to notify
  // the WebContentsImpl when an active connection indicator should be shown.
  mojo::ReceiverSet<device::mojom::SerialPortConnectionWatcher> watchers_;

  // Maps every receiver to a token to allow closing particular connections when
  // the user revokes a permission.
  std::multimap<const base::UnguessableToken, mojo::ReceiverId> watcher_ids_;

  base::WeakPtrFactory<SerialService> weak_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERIAL_SERIAL_SERVICE_H_
