// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USB_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_USB_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
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
  // the given frame.
  virtual void AdjustProtectedInterfaceClasses(
      RenderFrameHost& frame,
      std::vector<uint8_t>& classes) = 0;

  // Shows a chooser for the user to select a USB device.  |callback| will be
  // run when the prompt is closed. Deleting the returned object will cancel the
  // prompt. This method should not be called if CanRequestDevicePermission()
  // below returned false.
  virtual std::unique_ptr<UsbChooser> RunChooser(
      RenderFrameHost& frame,
      std::vector<device::mojom::UsbDeviceFilterPtr> filters,
      blink::mojom::WebUsbService::GetPermissionCallback callback) = 0;

  // Returns whether |frame| has permission to request access to a device.
  virtual bool CanRequestDevicePermission(RenderFrameHost& frame) = 0;

  virtual void RevokeDevicePermissionWebInitiated(
      content::RenderFrameHost& frame,
      const device::mojom::UsbDeviceInfo& device) = 0;

  virtual const device::mojom::UsbDeviceInfo* GetDeviceInfo(
      RenderFrameHost& frame,
      const std::string& guid) = 0;

  // Returns whether |frame| has permission to access |device|.
  virtual bool HasDevicePermission(
      RenderFrameHost& frame,
      const device::mojom::UsbDeviceInfo& device) = 0;

  // These two methods are expected to proxy to the UsbDeviceManager interface
  // owned by the embedder.
  //
  // Content and the embedder must use the same connection so that the embedder
  // can process connect/disconnect events for permissions management purposes
  // before they are delivered to content. Otherwise race conditions are
  // possible.
  virtual void GetDevices(
      RenderFrameHost& frame,
      blink::mojom::WebUsbService::GetDevicesCallback callback) = 0;
  virtual void GetDevice(
      RenderFrameHost& frame,
      const std::string& guid,
      base::span<const uint8_t> blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client) = 0;

  // Functions to manage the set of Observer instances registered to this
  // object.
  virtual void AddObserver(RenderFrameHost& frame, Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USB_DELEGATE_H_
