// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor_mojom_traits.h"
#include "third_party/blink/public/mojom/messaging/message_port_descriptor.mojom-blink.h"

namespace blink {

// This test lives in renderer/core because the serialization depends on other
// things in renderer/core. The main functionality of MessagePortDescriptor is
// tested in blink_common_unittests. For details, see
// blink/common/messaging/message_port_descriptor_unittest.cc
TEST(MessagePortDescriptorTest, SerializationWorks) {
  MessagePortDescriptorPair pair;
  MessagePortDescriptor port0 = pair.TakePort0();
  EXPECT_TRUE(port0.IsValid());

  base::UnguessableToken id = port0.id();
  uint64_t sequence_number = port0.sequence_number();

  // Do a round-trip through serialization and deserialization. This exercises
  // the custom StructTraits.
  MessagePortDescriptor port;
  mojo::test::SerializeAndDeserialize<mojom::blink::MessagePortDescriptor,
                                      MessagePortDescriptor>(&port0, &port);
  EXPECT_TRUE(port0.IsDefault());
  EXPECT_TRUE(port.IsValid());

  // Handles themselves can change IDs as they go through serialization, so we
  // don't explicitly test |raw_handle_|.
  EXPECT_EQ(id, port.id());
  EXPECT_EQ(sequence_number, port.sequence_number());
}

}  // namespace blink
