// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_api.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class BluetoothSocketApiUnittest : public ApiUnitTest {
 public:
  BluetoothSocketApiUnittest() = default;

  BluetoothSocketApiUnittest(const BluetoothSocketApiUnittest&) = delete;
  BluetoothSocketApiUnittest& operator=(const BluetoothSocketApiUnittest&) =
      delete;
};

// Tests that bluetoothSocket.create fails as expected when extension does not
// have permission.
TEST_F(BluetoothSocketApiUnittest, Permission) {
  auto function = base::MakeRefCounted<api::BluetoothSocketCreateFunction>();
  // Runs with an extension without bluetooth permission.
  EXPECT_EQ("Permission denied",
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Tests bluetoothSocket.create() and bluetoothSocket.close().
// Regression test for https://crbug.com/831651.
// TODO(crbug.com/40198321): Port //device/bluetooth to Fuchsia to enable
// bluetooth extensions.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_CreateThenClose DISABLED_CreateThenClose
#else
#define MAYBE_CreateThenClose CreateThenClose
#endif
TEST_F(BluetoothSocketApiUnittest, MAYBE_CreateThenClose) {
  scoped_refptr<const Extension> extension_with_socket_permitted =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "bluetooth app")
                  .Set("version", "1.0")
                  .Set("bluetooth", base::Value::Dict().Set("socket", true))
                  .Set("app", base::Value::Dict().Set(
                                  "background",
                                  base::Value::Dict().Set(
                                      "scripts", base::Value::List().Append(
                                                     "background.js")))))
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  ASSERT_TRUE(extension_with_socket_permitted);
  set_extension(extension_with_socket_permitted);

  auto create_function =
      base::MakeRefCounted<api::BluetoothSocketCreateFunction>();
  std::optional<base::Value> result =
      RunFunctionAndReturnValue(create_function.get(), "[]");
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());

  auto create_info =
      api::bluetooth_socket::CreateInfo::FromValue(result->GetDict());
  EXPECT_TRUE(create_info);

  const int socket_id = create_info->socket_id;
  auto close_function =
      base::MakeRefCounted<api::BluetoothSocketCloseFunction>();
  RunFunction(close_function.get(), base::StringPrintf("[%d]", socket_id));
}

}  // namespace extensions
