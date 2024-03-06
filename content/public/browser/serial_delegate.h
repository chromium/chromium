// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERIAL_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SERIAL_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/serial_chooser.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT SerialDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Events forwarded from SerialChooserContext::PortObserver:
    virtual void OnPortAdded(const device::mojom::SerialPortInfo& port) = 0;
    virtual void OnPortRemoved(const device::mojom::SerialPortInfo& port) = 0;
    virtual void OnPortConnectedStateChanged(
        const device::mojom::SerialPortInfo& port) = 0;
    virtual void OnPortManagerConnectionError() = 0;

    // Event forwarded from
    // permissions::ObjectPermissionContextBase::PermissionObserver:
    virtual void OnPermissionRevoked(const url::Origin& origin) = 0;
  };

  virtual ~SerialDelegate() = default;

  // Shows a chooser for the user to select a serial port.  |callback| will be
  // run when the prompt is closed. Deleting the returned object will cancel the
  // prompt. This method should not be called if CanRequestPortPermission()
  // below returned false.
  virtual std::unique_ptr<SerialChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids,
      SerialChooser::Callback callback) = 0;

  // Returns whether |frame| has permission to request access to a port.
  virtual bool CanRequestPortPermission(RenderFrameHost* frame) = 0;

  // Returns whether |frame| has permission to access |port|.
  virtual bool HasPortPermission(RenderFrameHost* frame,
                                 const device::mojom::SerialPortInfo& port) = 0;

  // Revokes |frame| permission to access port identified by |token| ordered by
  // website.
  virtual void RevokePortPermissionWebInitiated(
      RenderFrameHost* frame,
      const base::UnguessableToken& token) = 0;

  // Gets the port info for a particular port, identified by its |token|.
  virtual const device::mojom::SerialPortInfo* GetPortInfo(
      RenderFrameHost* frame,
      const base::UnguessableToken& token) = 0;

  // Returns an open connection to the SerialPortManager interface owned by
  // the embedder and being used to serve requests from |frame|.
  //
  // Content and the embedder must use the same connection so that the embedder
  // can process connect/disconnect events for permissions management purposes
  // before they are delivered to content. Otherwise race conditions are
  // possible.
  virtual device::mojom::SerialPortManager* GetPortManager(
      RenderFrameHost* frame) = 0;

  // Functions to manage the set of Observer instances registered to this
  // object.
  virtual void AddObserver(RenderFrameHost* frame, Observer* observer) = 0;
  virtual void RemoveObserver(RenderFrameHost* frame, Observer* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERIAL_DELEGATE_H_
