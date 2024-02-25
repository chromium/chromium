// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/hid_chooser.h"
#include "services/device/public/mojom/hid.mojom-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;
class RenderFrameHost;

class CONTENT_EXPORT HidDelegate {
 public:
  virtual ~HidDelegate() = default;

  class Observer : public base::CheckedObserver {
   public:
    // Events forwarded from HidChooserContext::DeviceObserver:
    virtual void OnDeviceAdded(const device::mojom::HidDeviceInfo&) = 0;
    virtual void OnDeviceRemoved(const device::mojom::HidDeviceInfo&) = 0;
    virtual void OnDeviceChanged(const device::mojom::HidDeviceInfo&) = 0;
    virtual void OnHidManagerConnectionError() = 0;

    // Event forwarded from
    // permissions::ObjectPermissionContextBase::PermissionObserver:
    virtual void OnPermissionRevoked(const url::Origin& origin) = 0;
  };

  // Shows a chooser for the user to select a HID device. |callback| will be
  // run when the prompt is closed. Deleting the returned object will cancel the
  // prompt.
  // If |filters| is empty, all connected devices are included in the chooser
  // list except devices that match one or more filters in |exclusion_filters|.
  // If |filters| is non-empty, connected devices are included if they match one
  // or more filters in |filters| and do not match any filters in
  // |exclusion_filters|.
  // This method should not be called if CanRequestDevicePermission() below
  // returned false.
  virtual std::unique_ptr<HidChooser> RunChooser(
      RenderFrameHost* render_frame_host,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      HidChooser::Callback callback) = 0;

  // Returns whether `origin` has permission to request access to a device.
  virtual bool CanRequestDevicePermission(BrowserContext* browser_context,
                                          const url::Origin& origin) = 0;

  // Returns whether `origin` has permission to access `device`.
  // Note that the method takes both render frame host and browser context, as
  // 'render_frame_host' is null for service workers.
  virtual bool HasDevicePermission(
      BrowserContext* browser_context,
      RenderFrameHost* render_frame_host,
      const url::Origin& origin,
      const device::mojom::HidDeviceInfo& device) = 0;

  // Revoke `device` access permission to `origin`.
  // Note that the method takes both render frame host and browser context, as
  // 'render_frame_host' is null for service workers.
  virtual void RevokeDevicePermission(
      BrowserContext* browser_context,
      RenderFrameHost* render_frame_host,
      const url::Origin& origin,
      const device::mojom::HidDeviceInfo& device) = 0;

  // Returns an open connection to the HidManager interface owned by the
  // embedder and being used to serve requests associated with
  // `browser_context`.
  //
  // Content and the embedder must use the same connection so that the embedder
  // can process connect/disconnect events for permissions management purposes
  // before they are delivered to content. Otherwise race conditions are
  // possible.
  virtual device::mojom::HidManager* GetHidManager(
      BrowserContext* browser_context) = 0;

  // Functions to manage the set of Observer instances registered to this
  // object.
  virtual void AddObserver(BrowserContext* browser_context,
                           Observer* observer) = 0;
  virtual void RemoveObserver(BrowserContext* browser_context,
                              Observer* observer) = 0;

  // Returns true if |origin| is allowed to bypass the HID blocklist and
  // access reports contained in FIDO collections.
  virtual bool IsFidoAllowedForOrigin(BrowserContext* browser_context,
                                      const url::Origin& origin) = 0;

  // Returns true if |origin| is allowed to access HID from service workers.
  virtual bool IsServiceWorkerAllowedForOrigin(const url::Origin& origin) = 0;

  // Gets the device info for a particular device, identified by its guid.
  virtual const device::mojom::HidDeviceInfo* GetDeviceInfo(
      BrowserContext* browser_context,
      const std::string& guid) = 0;

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

#endif  // CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_
