// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_messages_param_traits.h"

#include "extensions/common/mojom/host_id.mojom.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionsParamTraitsTest, HostIDPtrTest) {
  constexpr char kTestId[] = "aaaaaaaaaaaaa";

  extensions::mojom::HostIDPtr params = extensions::mojom::HostID::New();
  params->type = extensions::mojom::HostID::HostType::kWebUi;
  params->id = kTestId;

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, params);

  base::PickleIterator iter(msg);
  extensions::mojom::HostIDPtr output;

  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  EXPECT_EQ(output->type, extensions::mojom::HostID::HostType::kWebUi);
  EXPECT_EQ(output->id, kTestId);
}
