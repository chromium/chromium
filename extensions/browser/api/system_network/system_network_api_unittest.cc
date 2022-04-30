// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_network/system_network_api.h"
#include "build/build_config.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension_builder.h"

using extensions::api_test_utils::RunFunctionAndReturnSingleResult;
using extensions::api::SystemNetworkGetNetworkInterfacesFunction;
using extensions::api::system_network::NetworkInterface;

namespace extensions {

namespace {

using SystemNetworkApiUnitTest = extensions::ApiUnitTest;

}  // namespace

// TODO(crbug.com/1255187): Fails on Fuchsia running with run-test-component.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_GetNetworkInterfaces DISABLED_GetNetworkInterfaces
#else
#define MAYBE_GetNetworkInterfaces GetNetworkInterfaces
#endif  // BUILDFLAG(IS_FUCHSIA)
TEST_F(SystemNetworkApiUnitTest, MAYBE_GetNetworkInterfaces) {
  scoped_refptr<SystemNetworkGetNetworkInterfacesFunction> socket_function(
      new SystemNetworkGetNetworkInterfacesFunction());
  scoped_refptr<const Extension> empty_extension(
      extensions::ExtensionBuilder("Test").Build());

  socket_function->set_extension(empty_extension.get());
  socket_function->set_has_callback(true);

  std::unique_ptr<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_function.get(), "[]", browser_context()));
  ASSERT_TRUE(result->is_list());

  // All we can confirm is that we have at least one address, but not what it
  // is.
  ASSERT_FALSE(result->GetList().empty());

  for (const auto& network_interface_value : result->GetList()) {
    NetworkInterface network_interface;
    ASSERT_TRUE(NetworkInterface::Populate(network_interface_value,
                                           &network_interface));

    LOG(INFO) << "Network interface: address=" << network_interface.address
              << ", name=" << network_interface.name
              << ", prefix length=" << network_interface.prefix_length;
    ASSERT_NE(std::string(), network_interface.address);
    ASSERT_NE(std::string(), network_interface.name);
    ASSERT_LE(0, network_interface.prefix_length);
  }
}

}  // namespace extensions
