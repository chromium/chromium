// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_serialized_script_value.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/cloneable_message.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/messaging/cloneable_message.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(WebSerializedScriptValueTest, CloneableMessageRoundTrip) {
  test::TaskEnvironment task_environment;

  // Create a CloneableMessage with some test data.
  CloneableMessage input;
  // Use version 17 (0x11) to avoid byte swapping issues in tests.
  // Format: [`kVersionTag`, 17, 0 (Padding), 1, 2, 3, 4]
  input.owned_encoded_message = {0xFF, 0x11, 0x00, 0x01, 0x02, 0x03, 0x04};
  input.encoded_message = input.owned_encoded_message;
  base::UnguessableToken input_sender_agent_cluster_id =
      base::UnguessableToken::Create();
  input.sender_agent_cluster_id = input_sender_agent_cluster_id;

  auto blob = mojom::SerializedBlob::New();
  blob->uuid = "uuid";
  blob->content_type = "text/plain";
  blob->size = 10;
  mojo::PendingRemote<mojom::Blob> blob_remote;
  std::ignore = blob_remote.InitWithNewPipeAndPassReceiver();
  blob->blob = std::move(blob_remote);
  input.blobs.push_back(std::move(blob));

  // Convert to WebSerializedScriptValue.
  WebSerializedScriptValue serialized_value =
      WebSerializedScriptValue::CreateFromCloneableMessage(std::move(input));

  // Convert back to CloneableMessage.
  CloneableMessage output =
      serialized_value.GetCloneableMessage(input_sender_agent_cluster_id);

  // Verify the data survived the round trip.
  const uint8_t expected_data[] = {0xFF, 0x11, 0x00, 0x01, 0x02, 0x03, 0x04};
  EXPECT_EQ(output.encoded_message, base::span(expected_data));
  EXPECT_EQ(output.sender_agent_cluster_id, input_sender_agent_cluster_id);
  ASSERT_EQ(output.blobs.size(), 1u);
  EXPECT_EQ(output.blobs[0]->uuid, "uuid");
  EXPECT_EQ(output.blobs[0]->content_type, "text/plain");
  EXPECT_EQ(output.blobs[0]->size, 10u);
}

}  // namespace blink
