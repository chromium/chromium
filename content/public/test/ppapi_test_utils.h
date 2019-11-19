// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PPAPI_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_PPAPI_TEST_UTILS_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace base {
class CommandLine;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

// This file specifies utility functions used in Pepper testing in
// browser_tests and content_browsertests.
namespace ppapi {

// Registers the PPAPI test plugin to application/x-ppapi-tests. Returns true
// on success, and false otherwise.
bool RegisterTestPlugin(base::CommandLine* command_line) WARN_UNUSED_RESULT;

// Registers the PPAPI test plugin with some some extra parameters. Returns true
// on success and false otherwise.
bool RegisterTestPluginWithExtraParameters(
    base::CommandLine* command_line,
    const base::FilePath::StringType& extra_registration_parameters)
    WARN_UNUSED_RESULT;

// Registers the Flash-imitating CORB-testing plugin.
bool RegisterCorbTestPlugin(base::CommandLine* command_line) WARN_UNUSED_RESULT;

// Registers the Flash-imitating Power-Saver-testing plugin.
bool RegisterFlashTestPlugin(base::CommandLine* command_line)
    WARN_UNUSED_RESULT;

// Registers the Blink test plugin to application/x-blink-test-plugin.
bool RegisterBlinkTestPlugin(base::CommandLine* command_line)
    WARN_UNUSED_RESULT;

using CreateUDPSocketCallback = base::RepeatingCallback<void(
    network::mojom::NetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::UDPSocket> socket_receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> socket_listener)>;

// Sets a NetworkContext to be used by the Pepper TCP classes for testing.
// Passed in NetworkContext must remain valid until the method is called again
// with a nullptr, to clear the callback.
void SetPepperTCPNetworkContextForTesting(
    network::mojom::NetworkContext* network_context);

// Sets callback to be invoked when creating a UDPSocket for use by pepper.
// Passed in callback must remain valid until the method is called again with
// a nullptr, to clear the callback.
void SetPepperUDPSocketCallackForTesting(
    const CreateUDPSocketCallback* create_udp_socket_callback);

}  // namespace ppapi

#endif  // CONTENT_PUBLIC_TEST_PPAPI_TEST_UTILS_H_
