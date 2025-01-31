// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/serial/serial_service.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/serial_chooser.h"
#include "content/public/browser/serial_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace content {

SerialService::SerialService(RenderFrameHost* rfh)
    : DocumentUserData<SerialService>(rfh) {
  DCHECK(render_frame_host().IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature::kSerial));
  // Serial API is not supported for back-forward cache for now because we
  // don't have support for closing/freezing ports when the frame is added to
  // the back-forward cache, so we mark frames that use this API as disabled
  // for back-forward cache.
  BackForwardCache::DisableForRenderFrameHost(
      &render_frame_host(),
      BackForwardCacheDisable::DisabledReason(
          BackForwardCacheDisable::DisabledReasonId::kSerial));

  watchers_.set_disconnect_handler(base::BindRepeating(
      &SerialService::OnWatcherConnectionError, base::Unretained(this)));

  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (delegate)
    delegate->AddObserver(&render_frame_host(), this);
}

SerialService::~SerialService() {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (delegate)
    delegate->RemoveObserver(&render_frame_host(), this);

  // The remaining watchers will be closed from this end.
  if (!watchers_.empty())
    DecrementActiveFrameCount();
}

void SerialService::Bind(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SerialService::SetClient(
    mojo::PendingRemote<blink::mojom::SerialServiceClient> client) {
  clients_.Add(std::move(client));
}

void SerialService::GetPorts(GetPortsCallback callback) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate) {
    std::move(callback).Run(std::vector<blink::mojom::SerialPortInfoPtr>());
    return;
  }

  delegate->GetPortManager(&render_frame_host())
      ->GetDevices(base::BindOnce(&SerialService::FinishGetPorts,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void SerialService::RequestPort(
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    const std::vector<::device::BluetoothUUID>&
        allowed_bluetooth_service_class_ids,
    RequestPortCallback callback) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!delegate->CanRequestPortPermission(&render_frame_host())) {
    std::move(callback).Run(nullptr);
    return;
  }

  chooser_ = delegate->RunChooser(
      &render_frame_host(), std::move(filters),
      allowed_bluetooth_service_class_ids,
      base::BindOnce(&SerialService::FinishRequestPort,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SerialService::OpenPort(
    const base::UnguessableToken& token,
    device::mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<device::mojom::SerialPortClient> client,
    OpenPortCallback callback) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  const auto* port = delegate->GetPortInfo(&render_frame_host(), token);
  if (!port || !delegate->HasPortPermission(&render_frame_host(), *port)) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  if (watchers_.empty()) {
    auto* web_contents_impl = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(&render_frame_host()));
    web_contents_impl->IncrementSerialActiveFrameCount();
  }

  mojo::PendingRemote<device::mojom::SerialPortConnectionWatcher> watcher;
  mojo::ReceiverId receiver_id =
      watchers_.Add(this, watcher.InitWithNewPipeAndPassReceiver());
  watcher_ids_.insert({token, receiver_id});

  delegate->GetPortManager(&render_frame_host())
      ->OpenPort(token, /*use_alternate_path=*/false, std::move(options),
                 std::move(client), std::move(watcher), std::move(callback));
}

void SerialService::ForgetPort(const base::UnguessableToken& token,
                               ForgetPortCallback callback) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (delegate) {
    delegate->RevokePortPermissionWebInitiated(&render_frame_host(), token);
  }
  std::move(callback).Run();
}

void SerialService::OnPortAdded(const device::mojom::SerialPortInfo& port) {
  // Notify clients that a connect event should be dispatched for an added port.
  //
  // In some cases `port` may be disconnected. Wired serial ports are always
  // connected when first added. Bluetooth serial ports are added in the
  // disconnected state if the underlying Bluetooth device is paired with the
  // system but the system has no open connections to the device.
  //
  // Do not notify clients if `port` is disconnected since it would cause a
  // disconnect event to be dispatched for a port that did not previously
  // receive a 'connect' event and hasn't been returned by getPorts().
  if (port.connected) {
    OnPortConnectedStateChanged(port);
  }
}

void SerialService::OnPortRemoved(const device::mojom::SerialPortInfo& port) {
  OnPortConnectedStateChanged(port);
}

void SerialService::OnPortConnectedStateChanged(
    const device::mojom::SerialPortInfo& port) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate || !delegate->HasPortPermission(&render_frame_host(), port)) {
    return;
  }

  for (const auto& client : clients_) {
    client->OnPortConnectedStateChanged(ToBlinkType(port));
  }
}

void SerialService::OnPortManagerConnectionError() {
  // Reflect the loss of the SerialPortManager connection into the renderer
  // in order to force caches to be cleared and connections to be
  // re-established.
  receivers_.Clear();
  clients_.Clear();
  chooser_.reset();

  if (!watchers_.empty()) {
    watchers_.Clear();
    DecrementActiveFrameCount();
  }
}

void SerialService::OnPermissionRevoked(const url::Origin& origin) {
  if (origin != render_frame_host().GetMainFrame()->GetLastCommittedOrigin())
    return;

  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  size_t watchers_removed =
      std::erase_if(watcher_ids_, [&](const auto& watcher_entry) {
        const auto* port =
            delegate->GetPortInfo(&render_frame_host(), watcher_entry.first);
        if (port && delegate->HasPortPermission(&render_frame_host(), *port))
          return false;

        watchers_.Remove(watcher_entry.second);
        return true;
      });

  if (watchers_removed > 0 && watchers_.empty())
    DecrementActiveFrameCount();
}

void SerialService::FinishGetPorts(
    GetPortsCallback callback,
    std::vector<device::mojom::SerialPortInfoPtr> ports) {
  std::vector<blink::mojom::SerialPortInfoPtr> result;
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate) {
    std::move(callback).Run(std::move(result));
    return;
  }

  for (const auto& port : ports) {
    if (delegate->HasPortPermission(&render_frame_host(), *port))
      result.push_back(ToBlinkType(*port));
  }

  std::move(callback).Run(std::move(result));
}

void SerialService::FinishRequestPort(RequestPortCallback callback,
                                      device::mojom::SerialPortInfoPtr port) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate || !port) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(ToBlinkType(*port));
}

void SerialService::OnWatcherConnectionError() {
  if (watchers_.empty())
    DecrementActiveFrameCount();

  // Clean up any associated |watcher_ids_| entries.
  std::erase_if(watcher_ids_, [&](const auto& watcher_entry) {
    return watcher_entry.second == watchers_.current_receiver();
  });
}

void SerialService::DecrementActiveFrameCount() {
  auto* web_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(&render_frame_host()));
  web_contents_impl->DecrementSerialActiveFrameCount();
}

blink::mojom::SerialPortInfoPtr SerialService::ToBlinkType(
    const device::mojom::SerialPortInfo& port) {
  auto info = blink::mojom::SerialPortInfo::New();
  std::optional<std::string> persistent_identifier;

  info->has_usb_vendor_id = port.has_vendor_id;
  if (port.has_vendor_id) {
    info->usb_vendor_id = port.vendor_id;
  }
  info->has_usb_product_id = port.has_product_id;
  if (port.has_product_id) {
    info->usb_product_id = port.product_id;
  }
  if (port.bluetooth_service_class_id) {
    info->bluetooth_service_class_id = port.bluetooth_service_class_id;
    // Mac address + service uuid can persistently identify a serial port.
    persistent_identifier = base::UTF16ToUTF8(port.path.LossyDisplayName()) +
                            info->bluetooth_service_class_id->value();
  }
  info->connected = port.connected;
  if (persistent_identifier) {
    auto it = token_map_.find(*persistent_identifier);
    if (it == token_map_.end()) {
      auto result = token_map_.insert({*persistent_identifier, port.token});
      CHECK(result.second);
      it = result.first;
    }
    info->token = it->second;
  } else {
    info->token = port.token;
  }
  return info;
}

DOCUMENT_USER_DATA_KEY_IMPL(SerialService);

}  // namespace content
