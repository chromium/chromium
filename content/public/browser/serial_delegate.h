// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERIAL_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SERIAL_DELEGATE_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/serial_chooser.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT SerialDelegate {
 public:
  virtual ~SerialDelegate() = default;

  // Shows a chooser for the user to select a serial port.  |callback| will be
  // run when the prompt is closed. Deleting the returned object will cancel the
  // prompt. This method should not be called if CanRequestPortPermission()
  // below returned false.
  virtual std::unique_ptr<SerialChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      SerialChooser::Callback callback) = 0;

  // Returns whether |frame| has permission to request access to a port.
  virtual bool CanRequestPortPermission(RenderFrameHost* frame) = 0;

  // Returns whether |frame| has permission to access |port|.
  virtual bool HasPortPermission(RenderFrameHost* frame,
                                 const device::mojom::SerialPortInfo& port) = 0;

  // Returns an open connection to the SerialPortManager interface owned by
  // the embedder and being used to serve requests from |frame|.
  //
  // Content and the embedder must use the same connection so that the embedder
  // can process connect/disconnect events for permissions management purposes
  // before they are delivered to content. Otherwise race conditions are
  // possible.
  virtual device::mojom::SerialPortManager* GetPortManager(
      RenderFrameHost* frame) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERIAL_DELEGATE_H_
