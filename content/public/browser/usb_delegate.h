// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USB_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_USB_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class Page;
class RenderFrameHost;
class UsbChooser;

// Interface provided by the content embedder to support the WebUSB API.
class CONTENT_EXPORT UsbDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device) = 0;
    virtual void OnDeviceRemoved(
        const device::mojom::UsbDeviceInfo& device) = 0;
    virtual void OnDeviceManagerConnectionError() = 0;
    virtual void OnPermissionRevoked(const url::Origin& origin) = 0;
  };

  virtual ~UsbDelegate() = default;

  // Allows the embedder to modify the set of protected interface classes for
  // the given frame. `frame` may be nullptr if the context has no frame.
  virtual void AdjustProtectedInterfaceClasses(
      BrowserContext* browser_context,
      const url::Origin& origin,
      RenderFrameHost* frame,
      std::vector<uint8_t>& classes) = 0;

  // Shows a chooser for the user to select a USB device.  |callback| will be
  // run when the prompt is closed. Deleting the returned object will cancel the
  // prompt. This method should not be called if CanRequestDevicePermission()
  // below returned false.
  virtual std::unique_ptr<UsbChooser> RunChooser(
      RenderFrameHost& frame,
      blink::mojom::WebUsbRequestDeviceOptionsPtr options,
      blink::mojom::WebUsbService::GetPermissionCallback callback) = 0;

  // By returning false, allows the embedder to deny WebUSB access to documents
  // of the given `page`. This is beyond the restrictions already enforced
  // within content/. For example, the embedder may be displaying `page` in a
  // context where prompting for permissions is not appropriate.
  virtual bool PageMayUseUsb(Page& page) = 0;

  // Returns whether `origin` in `browser_context` has permission to request
  // access to a device.
  virtual bool CanRequestDevicePermission(BrowserContext* browser_context,
                                          const url::Origin& origin) = 0;

  // Attempts to revoke the permission for `origin` in `browser_context` to
  // access the USB device described by `device`.
  virtual void RevokeDevicePermissionWebInitiated(
      BrowserContext* browser_context,
      const url::Origin& origin,
      const device::mojom::UsbDeviceInfo& device) = 0;

  // Returns device information for the device with matching `guid`.
  virtual const device::mojom::UsbDeviceInfo* GetDeviceInfo(
      BrowserContext* browser_context,
      const std::string& guid) = 0;

  // Returns whether `origin` in `browser_context` has permission to access
  // the USB device described by `device_info`.
  virtual bool HasDevicePermission(
      BrowserContext* browser_context,
      RenderFrameHost* frame,
      const url::Origin& origin,
      const device::mojom::UsbDeviceInfo& device_info) = 0;

  // These two methods are expected to proxy to the UsbDeviceManager interface
  // owned by the embedder.
  //
  // Content and the embedder must use the same connection so that the embedder
  // can process connect/disconnect events for permissions management purposes
  // before they are delivered to content. Otherwise race conditions are
  // possible.
  virtual void GetDevices(
      BrowserContext* browser_context,
      blink::mojom::WebUsbService::GetDevicesCallback callback) = 0;
  virtual void GetDevice(
      BrowserContext* browser_context,
      const std::string& guid,
      base::span<const uint8_t> blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client) = 0;

  // Functions to manage the set of Observer instances registered to this
  // object.
  virtual void AddObserver(BrowserContext* browser_context,
                           Observer* observer) = 0;
  virtual void RemoveObserver(BrowserContext* browser_context,
                              Observer* observer) = 0;

  // Returns true if `origin` is allowed to access WebUSB from service workers.
  virtual bool IsServiceWorkerAllowedForOrigin(const url::Origin& origin) = 0;

  // Notify the delegate a connection is created on |origin| by
  // |browser_context|.
  virtual void IncrementConnectionCount(BrowserContext* browser_context,
                                        const url::Origin& origin) = 0;
  // Notify the delegate a connection is closed on |origin| by
  // |browser_context|.
  virtual void DecrementConnectionCount(BrowserContext* browser_context,
                                        const url::Origin& origin) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USB_DELEGATE_H_
