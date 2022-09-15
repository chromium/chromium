// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/usb/web_usb_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace content {

// A UsbDeviceClient represents a UsbDevice pipe that has been passed to the
// renderer process. The UsbDeviceClient pipe allows the browser process to
// continue to monitor how the device is used and cause the connection to be
// closed at will.
class WebUsbServiceImpl::UsbDeviceClient
    : public device::mojom::UsbDeviceClient {
 public:
  UsbDeviceClient(
      WebUsbServiceImpl* service,
      const std::string& device_guid,
      mojo::PendingReceiver<device::mojom::UsbDeviceClient> receiver)
      : service_(service),
        device_guid_(device_guid),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebUsbServiceImpl::RemoveDeviceClient,
                       base::Unretained(service_), base::Unretained(this)));
  }

  ~UsbDeviceClient() override {
    if (opened_) {
      // If the connection was opened destroying |receiver_| will cause it to
      // be closed but that event won't be dispatched here because the receiver
      // has been destroyed.
      OnDeviceClosed();
    }
  }

  const std::string& device_guid() const { return device_guid_; }

  // device::mojom::UsbDeviceClient implementation:
  void OnDeviceOpened() override {
    DCHECK(!opened_);
    opened_ = true;
    service_->IncrementConnectionCount();
  }

  void OnDeviceClosed() override {
    DCHECK(opened_);
    opened_ = false;
    service_->DecrementConnectionCount();
  }

 private:
  const raw_ptr<WebUsbServiceImpl> service_;
  const std::string device_guid_;
  bool opened_ = false;
  mojo::Receiver<device::mojom::UsbDeviceClient> receiver_;
};

WebUsbServiceImpl::WebUsbServiceImpl(RenderFrameHost* frame)
    : DocumentUserData(frame) {
  auto* web_contents = WebContents::FromRenderFrameHost(&render_frame_host());
  // This class is destroyed on cross-origin navigations and so it is safe to
  // cache these values.
  origin_ = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  if (delegate())
    delegate()->AddObserver(render_frame_host(), this);
}

WebUsbServiceImpl::~WebUsbServiceImpl() {
  if (delegate())
    delegate()->RemoveObserver(this);
}

void WebUsbServiceImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

UsbDelegate* WebUsbServiceImpl::delegate() const {
  return GetContentClient()->browser()->GetUsbDelegate();
}

std::vector<uint8_t> WebUsbServiceImpl::GetProtectedInterfaceClasses() const {
  // Specified in https://wicg.github.io/webusb#protected-interface-classes
  std::vector<uint8_t> classes = {
      device::mojom::kUsbAudioClass,       device::mojom::kUsbHidClass,
      device::mojom::kUsbMassStorageClass, device::mojom::kUsbSmartCardClass,
      device::mojom::kUsbVideoClass,       device::mojom::kUsbAudioVideoClass,
      device::mojom::kUsbWirelessClass,
  };

  if (delegate())
    delegate()->AdjustProtectedInterfaceClasses(render_frame_host(), classes);

  return classes;
}

void WebUsbServiceImpl::GetDevices(GetDevicesCallback callback) {
  if (!delegate()) {
    std::move(callback).Run(std::vector<device::mojom::UsbDeviceInfoPtr>());
    return;
  }

  delegate()->GetDevices(
      render_frame_host(),
      base::BindOnce(&WebUsbServiceImpl::OnGetDevices,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebUsbServiceImpl::OnGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list) {
  DCHECK(delegate());

  std::vector<device::mojom::UsbDeviceInfoPtr> device_infos;
  for (auto& device_info : device_info_list) {
    if (delegate()->HasDevicePermission(render_frame_host(), *device_info)) {
      device_infos.push_back(device_info.Clone());
    }
  }
  std::move(callback).Run(std::move(device_infos));
}

void WebUsbServiceImpl::GetDevice(
    const std::string& guid,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) {
  if (!delegate()) {
    return;
  }

  auto* device_info = delegate()->GetDeviceInfo(render_frame_host(), guid);
  if (!device_info ||
      !delegate()->HasDevicePermission(render_frame_host(), *device_info)) {
    return;
  }

  // Connect Blink to the native device and keep a receiver to this for the
  // UsbDeviceClient interface so we can receive DeviceOpened/Closed events.
  // This receiver will also be closed to notify the device service to close
  // the connection if permission is revoked.
  mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client;
  device_clients_.push_back(std::make_unique<UsbDeviceClient>(
      this, guid, device_client.InitWithNewPipeAndPassReceiver()));

  delegate()->GetDevice(render_frame_host(), guid,
                        GetProtectedInterfaceClasses(),
                        std::move(device_receiver), std::move(device_client));
}

void WebUsbServiceImpl::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    GetPermissionCallback callback) {
  if (!delegate() ||
      !delegate()->CanRequestDevicePermission(render_frame_host())) {
    std::move(callback).Run(nullptr);
    return;
  }

  usb_chooser_ = delegate()->RunChooser(
      render_frame_host(), std::move(device_filters), std::move(callback));
}

void WebUsbServiceImpl::ForgetDevice(const std::string& guid,
                                     ForgetDeviceCallback callback) {
  if (delegate()) {
    auto* device_info = delegate()->GetDeviceInfo(render_frame_host(), guid);
    if (device_info &&
        delegate()->HasDevicePermission(render_frame_host(), *device_info)) {
      delegate()->RevokeDevicePermissionWebInitiated(render_frame_host(),
                                                     *device_info);
    }
  }
  std::move(callback).Run();
}

void WebUsbServiceImpl::SetClient(
    mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
        client) {
  DCHECK(client);
  clients_.Add(std::move(client));
}

void WebUsbServiceImpl::OnPermissionRevoked(const url::Origin& origin) {
  if (origin_ != origin) {
    return;
  }

  // Close the connection between Blink and the device if the device lost
  // permission.
  base::EraseIf(device_clients_, [this](const auto& client) {
    auto* device_info =
        delegate()->GetDeviceInfo(render_frame_host(), client->device_guid());
    if (!device_info)
      return true;

    return !delegate()->HasDevicePermission(render_frame_host(), *device_info);
  });
}

void WebUsbServiceImpl::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!delegate()->HasDevicePermission(render_frame_host(), device_info))
    return;

  for (auto& client : clients_)
    client->OnDeviceAdded(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::EraseIf(device_clients_, [&device_info](const auto& client) {
    return device_info.guid == client->device_guid();
  });

  if (!delegate()->HasDevicePermission(render_frame_host(), device_info))
    return;

  for (auto& client : clients_)
    client->OnDeviceRemoved(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceManagerConnectionError() {
  // Close the connection with blink.
  clients_.Clear();
  receivers_.Clear();
}

// device::mojom::UsbDeviceClient implementation:
void WebUsbServiceImpl::IncrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (connection_count_++ == 0) {
    auto* web_contents = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(&render_frame_host()));
    web_contents->IncrementUsbActiveFrameCount();
  }
}

void WebUsbServiceImpl::DecrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_GT(connection_count_, 0);
  if (--connection_count_ == 0) {
    auto* web_contents = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(&render_frame_host()));
    web_contents->DecrementUsbActiveFrameCount();
  }
}

void WebUsbServiceImpl::RemoveDeviceClient(const UsbDeviceClient* client) {
  base::EraseIf(device_clients_, [client](const auto& this_client) {
    return client == this_client.get();
  });
}

DOCUMENT_USER_DATA_KEY_IMPL(WebUsbServiceImpl);

}  // namespace content
