// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_MESSAGE_PORT_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_MESSAGE_PORT_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/fidl/cpp/interface_request.h>

#include <optional>

#include "third_party/blink/public/common/messaging/web_message_port.h"

// Creates a connected MessagePort from a FIDL MessagePort request and
// returns a handle to its peer blink::WebMessagePort.
blink::WebMessagePort BlinkMessagePortFromFidl(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> fidl_port);

// Creates a connected MessagePort from a remote FIDL MessagePort handle,
// returns a handle to its peer Mojo pipe.
blink::WebMessagePort BlinkMessagePortFromFidl(
    fidl::InterfaceHandle<fuchsia::web::MessagePort> fidl_port);

// Creates a connected MessagePort from a transferred blink::WebMessagePort and
// returns a handle to its FIDL interface peer.
fidl::InterfaceHandle<fuchsia::web::MessagePort> FidlMessagePortFromBlink(
    blink::WebMessagePort blink_port);

// Specifies the location of the MessagePort FIDL service that handles messages
// sent over the Transferable.
enum class TransferableHostType {
  // The MessagePort FIDL service is hosted in-process.
  kLocal,

  // The MessagePort FIDL service is hosted remotely.
  kRemote,
};

// Converts a BlinkMessage to a fuchsia::web::WebMessage.
std::optional<fuchsia::web::WebMessage> FidlWebMessageFromBlink(
    blink::WebMessagePort::Message blink_message,
    TransferableHostType port_type);

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_MESSAGE_PORT_H_
