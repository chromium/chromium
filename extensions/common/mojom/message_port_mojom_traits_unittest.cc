// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/message_port_mojom_traits.h"

#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace extensions {

TEST(MessagePortMojomTraitsTest, PortId) {
  PortId input(base::UnguessableToken::Create(), /*port_number=*/32,
               /*is_opener=*/true,
               mojom::SerializationFormat::kStructuredClone);
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

  EXPECT_TRUE(input.EqualsForTesting(output));
  EXPECT_EQ(input.from_privileged_context(), output.from_privileged_context());
}

TEST(MessagePortMojomTraitsTest, JSONMessageValues) {
  Message input("some text", /*user_gesture=*/true,
                /*from_privileged_context=*/true);
  Message output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<extensions::mojom::Message>(
      input, output));

  EXPECT_TRUE(input.EqualsForTesting(output));
  EXPECT_EQ(input.from_privileged_context(), output.from_privileged_context());
}

TEST(MessagePortMojomTraitsTest, MessageDataJson) {
  MessageData input = std::string("json data");
  MessageData output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<extensions::mojom::MessageData>(
          input, output));
  EXPECT_TRUE(std::holds_alternative<std::string>(output));
  EXPECT_EQ(std::get<std::string>(input), std::get<std::string>(output));
}

class StructuredMessagePortMojomTraitsTest : public testing::Test {
 public:
  StructuredMessagePortMojomTraitsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kStructuredCloningForMessaging);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(StructuredMessagePortMojomTraitsTest, StructuredMessageValues) {
  const std::string kData("text");
  StructuredCloneMessageData message_data;
  message_data.owned_encoded_message =
      std::vector<uint8_t>(kData.begin(), kData.end());
  message_data.encoded_message = message_data.owned_encoded_message;
  message_data.sender_agent_cluster_id = base::UnguessableToken::Create();

  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  std::ignore = blob_remote.InitWithNewPipeAndPassReceiver();
  message_data.blobs.push_back(blink::mojom::SerializedBlob::New(
      "uuid", "content_type", /*size=*/10, std::move(blob_remote)));

  Message input(std::move(message_data), /*user_gesture=*/true,
                /*from_privileged_context=*/true);
  Message output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<extensions::mojom::Message>(
      input, output));

  EXPECT_TRUE(input.EqualsForTesting(output));
  EXPECT_EQ(input.from_privileged_context(), output.from_privileged_context());
}

TEST_F(StructuredMessagePortMojomTraitsTest, MessageDataStructured) {
  const std::string kData("text");
  StructuredCloneMessageData message_data;
  message_data.owned_encoded_message =
      std::vector<uint8_t>(kData.begin(), kData.end());
  message_data.encoded_message = message_data.owned_encoded_message;
  message_data.sender_agent_cluster_id = base::UnguessableToken::Create();

  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  std::ignore = blob_remote.InitWithNewPipeAndPassReceiver();
  message_data.blobs.push_back(blink::mojom::SerializedBlob::New(
      "uuid", "content_type", /*size=*/10, std::move(blob_remote)));

  MessageData input_data = std::move(message_data);
  MessageData output_data;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<extensions::mojom::MessageData>(
          input_data, output_data));

  // We can't compare `MessageData` directly, so wrap them in `Message` objects
  // and use `Message::EqualsForTesting()` which handles blob comparison.
  Message input(std::move(input_data), /*user_gesture=*/true,
                /*from_privileged_context=*/true);
  Message output(std::move(output_data), /*user_gesture=*/true,
                 /*from_privileged_context=*/true);

  EXPECT_TRUE(input.EqualsForTesting(output));
}

}  // namespace extensions
