// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/message_port_mojom_traits.h"

#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(MessagePortMojomTraitsTest, PortId) {
  PortId input(base::UnguessableToken::Create(), 32, true,
               mojom::SerializationFormat::kStructuredCloned);
  PortId output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<extensions::mojom::PortId>(
      input, output));

  EXPECT_EQ(input.context_id, output.context_id);
  EXPECT_EQ(input.port_number, output.port_number);
  EXPECT_EQ(input.is_opener, output.is_opener);
  EXPECT_EQ(input.serialization_format, output.serialization_format);
}

TEST(MessagePortMojomTraitsTest, MessagingEndpointEmpty) {
  MessagingEndpoint input;
  MessagingEndpoint output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<extensions::mojom::MessagingEndpoint>(
          input, output));

  EXPECT_EQ(input.type, output.type);
  EXPECT_EQ(input.extension_id, output.extension_id);
  EXPECT_EQ(input.native_app_name, output.native_app_name);
}

TEST(MessagePortMojomTraitsTest, MessagingEndpointValues) {
  MessagingEndpoint input;
  input.type = MessagingEndpoint::Type::kContentScript;
  input.extension_id = "foo";
  input.native_app_name = "bar";
  MessagingEndpoint output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<extensions::mojom::MessagingEndpoint>(
          input, output));

  EXPECT_EQ(input.type, output.type);
  EXPECT_EQ(input.extension_id, output.extension_id);
  EXPECT_EQ(input.native_app_name, output.native_app_name);
}

TEST(MessagePortMojomTraitsTest, MessageEmpty) {
  Message input;
  Message output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<extensions::mojom::Message>(
      input, output));

  EXPECT_EQ(input.data, output.data);
  EXPECT_EQ(input.format, output.format);
  EXPECT_EQ(input.user_gesture, output.user_gesture);
  EXPECT_EQ(input.from_privileged_context, output.from_privileged_context);
}

TEST(MessagePortMojomTraitsTest, MessageValues) {
  Message input;
  input.data = "some text";
  input.format = mojom::SerializationFormat::kStructuredCloned;
  input.user_gesture = true;
  input.from_privileged_context = true;
  Message output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<extensions::mojom::Message>(
      input, output));

  EXPECT_EQ(input.data, output.data);
  EXPECT_EQ(input.format, output.format);
  EXPECT_EQ(input.user_gesture, output.user_gesture);
  EXPECT_EQ(input.from_privileged_context, output.from_privileged_context);

  input.user_gesture = false;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<extensions::mojom::Message>(
      input, output));

  EXPECT_EQ(input.data, output.data);
  EXPECT_EQ(input.format, output.format);
  EXPECT_EQ(input.user_gesture, output.user_gesture);
  EXPECT_EQ(input.from_privileged_context, output.from_privileged_context);
}

}  // namespace extensions
