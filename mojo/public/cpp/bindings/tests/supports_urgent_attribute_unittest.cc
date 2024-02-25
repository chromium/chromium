// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/lib/control_message_handler.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/supports_urgent_attribute_unittest.test-mojom.h"
#include "mojo/public/cpp/bindings/urgent_message_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test::supports_urgent_attribute_unittest {

class UrgentMessageFilter : public MessageFilter {
 public:
  UrgentMessageFilter() = default;
  ~UrgentMessageFilter() override = default;

  size_t num_urgent_messages() { return num_urgent_messges_; }

  size_t num_non_urgent_messages() { return num_non_urgent_messges_; }

  // MessageFilter:
  bool WillDispatch(Message* message) override {
    // Ignore control messages sent during flush.
    if (!internal::ControlMessageHandler::IsControlMessage(message)) {
      if (message->has_flag(Message::kFlagIsUrgent)) {
        ++num_urgent_messges_;
      } else {
        ++num_non_urgent_messges_;
      }
    }
    return true;
  }

  void DidDispatchOrReject(Message* message, bool accepted) override {}

 private:
  size_t num_urgent_messges_ = 0;
  size_t num_non_urgent_messges_ = 0;
};

class TestInterfaceImpl : public mojom::TestInterface {
 public:
  TestInterfaceImpl() = default;
  ~TestInterfaceImpl() override = default;

  TestInterfaceImpl(const TestInterfaceImpl&) = delete;
  TestInterfaceImpl& operator=(const TestInterfaceImpl&) = delete;

 private:
  // mojom::TestInterface:
  void MaybeUrgentMessage() override {}
  void NonUrgentMessage() override {}
};

using SupportsUrgentAttributeTest = BindingsTestBase;

TEST_P(SupportsUrgentAttributeTest, TestAttribute) {
  TestInterfaceImpl impl;
  Remote<mojom::TestInterface> remote;
  Receiver<mojom::TestInterface> receiver(&impl,
                                          remote.BindNewPipeAndPassReceiver());

  auto filter = std::make_unique<UrgentMessageFilter>();
  UrgentMessageFilter* unowned_filter = filter.get();
  receiver.SetFilter(std::move(filter));

  EXPECT_EQ(unowned_filter->num_urgent_messages(), 0u);
  EXPECT_EQ(unowned_filter->num_non_urgent_messages(), 0u);

  {
    UrgentMessageScope scope;
    remote->MaybeUrgentMessage();
  }
  remote.FlushForTesting();

  EXPECT_EQ(unowned_filter->num_urgent_messages(), 1u);
  EXPECT_EQ(unowned_filter->num_non_urgent_messages(), 0u);

  remote->NonUrgentMessage();
  remote.FlushForTesting();

  EXPECT_EQ(unowned_filter->num_urgent_messages(), 1u);
  EXPECT_EQ(unowned_filter->num_non_urgent_messages(), 1u);

  remote->MaybeUrgentMessage();
  remote.FlushForTesting();

  EXPECT_EQ(unowned_filter->num_urgent_messages(), 1u);
  EXPECT_EQ(unowned_filter->num_non_urgent_messages(), 2u);

  {
    UrgentMessageScope scope;
    remote->MaybeUrgentMessage();
    remote->NonUrgentMessage();
  }
  remote.FlushForTesting();

  EXPECT_EQ(unowned_filter->num_urgent_messages(), 2u);
  EXPECT_EQ(unowned_filter->num_non_urgent_messages(), 3u);

  unowned_filter = nullptr;
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(SupportsUrgentAttributeTest);

}  // namespace mojo::test::supports_urgent_attribute_unittest
