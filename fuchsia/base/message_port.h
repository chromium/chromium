// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_MESSAGE_PORT_H_
#define FUCHSIA_BASE_MESSAGE_PORT_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/fidl/cpp/interface_request.h>
#include <memory>

#include "third_party/blink/public/common/messaging/web_message_port.h"

namespace cr_fuchsia {

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

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_MESSAGE_PORT_H_
