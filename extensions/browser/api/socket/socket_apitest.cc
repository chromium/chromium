// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_test.h"

using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

namespace extensions {

class SocketApiTest : public AppShellTest {};

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketUDPCreateGood) {
  scoped_refptr<extensions::SocketCreateFunction> socket_create_function(
      new extensions::SocketCreateFunction());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_create_function.get(), "[\"udp\"]", browser_context()));
  const base::Value::Dict& value = result->GetDict();
  std::optional<int> socket_id = value.FindInt("socketId");
  ASSERT_TRUE(socket_id);
  EXPECT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketTCPCreateGood) {
  scoped_refptr<extensions::SocketCreateFunction> socket_create_function(
      new extensions::SocketCreateFunction());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_create_function.get(), "[\"tcp\"]", browser_context()));
  const base::Value::Dict& value = result->GetDict();
  std::optional<int> socket_id = value.FindInt("socketId");
  ASSERT_TRUE(socket_id);
  ASSERT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, GetNetworkList) {
  scoped_refptr<extensions::SocketGetNetworkListFunction> socket_function(
      new extensions::SocketGetNetworkListFunction());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_function->set_extension(empty_extension.get());
  socket_function->set_has_callback(true);

  std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_function.get(), "[]", browser_context()));

  // If we're invoking socket tests, all we can confirm is that we have at
  // least one address, but not what it is.
  ASSERT_TRUE(result->is_list());
  ASSERT_FALSE(result->GetList().empty());
}

}  //  namespace extensions
