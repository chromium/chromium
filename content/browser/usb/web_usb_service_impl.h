// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
#define CONTENT_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/usb_chooser.h"
#include "content/public/browser/usb_delegate.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImpl;

// Implements a restricted device::mojom::UsbDeviceManager interface by wrapping
// another UsbDeviceManager instance and enforces the rules of the WebUSB
// permission model as well as permission granted by the user through a device
// chooser UI.
class CONTENT_EXPORT WebUsbServiceImpl : public blink::mojom::WebUsbService,
                                         public UsbDelegate::Observer {
 public:
  WebUsbServiceImpl(RenderFrameHostImpl* render_frame_host,
                    base::WeakPtr<ServiceWorkerVersion> service_worker_version,
                    const url::Origin& origin);
  WebUsbServiceImpl(const WebUsbServiceImpl&) = delete;
  WebUsbServiceImpl& operator=(const WebUsbServiceImpl&) = delete;
  ~WebUsbServiceImpl() override;

  // Use this when creating from a document.
  static void Create(
      RenderFrameHostImpl& render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebUsbService> pending_receiver);

  // Use this when creating from a service worker.
  static void Create(
      base::WeakPtr<ServiceWorkerVersion> service_worker_version,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::WebUsbService> pending_receiver);

  // blink::mojom::WebUsbService implementation:
  void GetDevices(GetDevicesCallback callback) override;
  void GetDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) override;
  void GetPermission(blink::mojom::WebUsbRequestDeviceOptionsPtr options,
                     GetPermissionCallback callback) override;
  void ForgetDevice(const std::string& guid,
                    ForgetDeviceCallback callback) override;
  void SetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client) override;

  // UsbDelegate::Observer implementation:
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceRemoved(
      const device::mojom::UsbDeviceInfo& device_info) override;
  void OnDeviceManagerConnectionError() override;
  void OnPermissionRevoked(const url::Origin& origin) override;

  const mojo::AssociatedRemoteSet<device::mojom::UsbDeviceManagerClient>&
  clients() const {
    return clients_;
  }

  base::WeakPtr<content::ServiceWorkerVersion> service_worker_version() {
    return service_worker_version_;
  }

 private:
  class UsbDeviceClient;

  // Get the `BrowserContext` this `WebUsbServiceImpl` belongs to. Returns
  // `nullptr` if the `BrowserContext` is destroyed.
  BrowserContext* GetBrowserContext() const;

  std::vector<uint8_t> GetProtectedInterfaceClasses() const;

  void OnGetDevices(
      GetDevicesCallback callback,
      std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list);

  void IncrementConnectionCount();
  void DecrementConnectionCount();
  void RemoveDeviceClient(const UsbDeviceClient* client);

  // May be `nullptr` if this `WebUsbServiceImpl` is created in a context
  // without a frame. When `render_frame_host_` is destroyed, this
  // `WebUsbServiceImpl` is destroyed first.
  const raw_ptr<RenderFrameHostImpl> render_frame_host_;

  // The ServiceWorkerVersion of the service worker this WebUsbService belongs
  // to.
  const base::WeakPtr<ServiceWorkerVersion> service_worker_version_;

  // The request uuid for keeping service worker alive.
  std::optional<base::Uuid> service_worker_activity_request_uuid_;

  const url::Origin origin_;

  std::unique_ptr<UsbChooser> usb_chooser_;

  // Used to bind with Blink.
  mojo::AssociatedRemoteSet<device::mojom::UsbDeviceManagerClient> clients_;

  // A UsbDeviceClient tracks a UsbDevice pipe that has been passed to Blink.
  int connection_count_ = 0;
  std::vector<std::unique_ptr<UsbDeviceClient>> device_clients_;

  base::WeakPtrFactory<WebUsbServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_USB_WEB_USB_SERVICE_IMPL_H_
