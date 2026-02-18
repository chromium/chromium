// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/multiple_durable_message_writer_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "services/network/devtools_durable_msg.h"
#include "services/network/devtools_durable_msg_accounting_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class MockDevtoolsDurableMessageAccountingDelegate
    : public DevtoolsDurableMessageAccountingDelegate {
 public:
  MOCK_METHOD(void,
              WillAddBytes,
              (DevtoolsDurableMessage & message, int64_t chunk_size),
              (override));
  MOCK_METHOD(void,
              WillRemoveBytes,
              (DevtoolsDurableMessage & message),
              (override));
  MOCK_METHOD(void,
              WillDestroyMessage,
              (DevtoolsDurableMessage & message),
              (override));
};

}  // namespace

class MultipleDurableMessageWriterImplTest : public testing::Test {
 public:
  MultipleDurableMessageWriterImplTest() = default;
  ~MultipleDurableMessageWriterImplTest() override = default;

 protected:
  MockDevtoolsDurableMessageAccountingDelegate accounting_delegate_;
};

TEST_F(MultipleDurableMessageWriterImplTest, ForwardsAddBytes) {
  auto msg1 =
      std::make_unique<DevtoolsDurableMessage>("req1", accounting_delegate_);
  auto msg2 =
      std::make_unique<DevtoolsDurableMessage>("req2", accounting_delegate_);

  std::vector<base::WeakPtr<DevtoolsDurableMessage>> messages;
  messages.push_back(msg1->GetWeakPtr());
  messages.push_back(msg2->GetWeakPtr());

  MultipleDurableMessageWriterImpl writer(std::move(messages));

  std::string data = "test data";
  base::span<const uint8_t> bytes = base::as_byte_span(data);

  // Expect WillAddBytes to be called for both messages
  EXPECT_CALL(accounting_delegate_,
              WillAddBytes(testing::Ref(*msg1), bytes.size()));
  EXPECT_CALL(accounting_delegate_,
              WillAddBytes(testing::Ref(*msg2), bytes.size()));

  writer.AddBytes(bytes, bytes.size());

  EXPECT_EQ(msg1->byte_size_for_testing(), bytes.size());
  EXPECT_EQ(msg2->byte_size_for_testing(), bytes.size());
}

TEST_F(MultipleDurableMessageWriterImplTest, ForwardsMarkComplete) {
  auto msg1 =
      std::make_unique<DevtoolsDurableMessage>("req1", accounting_delegate_);
  auto msg2 =
      std::make_unique<DevtoolsDurableMessage>("req2", accounting_delegate_);

  std::vector<base::WeakPtr<DevtoolsDurableMessage>> messages;
  messages.push_back(msg1->GetWeakPtr());
  messages.push_back(msg2->GetWeakPtr());

  MultipleDurableMessageWriterImpl writer(std::move(messages));

  writer.MarkComplete();

  EXPECT_TRUE(msg1->is_complete());
  EXPECT_TRUE(msg2->is_complete());
}

TEST_F(MultipleDurableMessageWriterImplTest, ForwardsSetClientDecodingTypes) {
  auto msg1 =
      std::make_unique<DevtoolsDurableMessage>("req1", accounting_delegate_);
  auto msg2 =
      std::make_unique<DevtoolsDurableMessage>("req2", accounting_delegate_);

  std::vector<base::WeakPtr<DevtoolsDurableMessage>> messages;
  messages.push_back(msg1->GetWeakPtr());
  messages.push_back(msg2->GetWeakPtr());

  MultipleDurableMessageWriterImpl writer(std::move(messages));

  std::vector<net::SourceStreamType> types = {net::SourceStreamType::kGzip};
  writer.SetClientDecodingTypes(types);

  EXPECT_EQ(msg1->get_client_decoding_types_for_testing(), types);
  EXPECT_EQ(msg2->get_client_decoding_types_for_testing(), types);
}

TEST_F(MultipleDurableMessageWriterImplTest, HandlesDestroyedMessages) {
  auto msg1 =
      std::make_unique<DevtoolsDurableMessage>("req1", accounting_delegate_);
  auto msg2 =
      std::make_unique<DevtoolsDurableMessage>("req2", accounting_delegate_);

  std::vector<base::WeakPtr<DevtoolsDurableMessage>> messages;
  messages.push_back(msg1->GetWeakPtr());
  messages.push_back(msg2->GetWeakPtr());

  MultipleDurableMessageWriterImpl writer(std::move(messages));

  // Destroy msg1
  msg1.reset();

  std::string data = "test data";
  base::span<const uint8_t> bytes = base::as_byte_span(data);

  // Expect WillAddBytes to be called only for msg2
  EXPECT_CALL(accounting_delegate_,
              WillAddBytes(testing::Ref(*msg2), bytes.size()));

  writer.AddBytes(bytes, bytes.size());
  writer.MarkComplete();

  EXPECT_TRUE(msg2->is_complete());
  EXPECT_EQ(msg2->byte_size_for_testing(), bytes.size());
}

}  // namespace network
