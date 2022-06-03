// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_BYTES_CONSUMER_TEST_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_BYTES_CONSUMER_TEST_READER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace scheduler {
class FakeTaskRunner;
}  // namespace scheduler

class BytesConsumerTestReader final
    : public GarbageCollected<BytesConsumerTestReader>,
      public BytesConsumer::Client {
 public:
  // |consumer| must not have a client when called.
  explicit BytesConsumerTestReader(BytesConsumer* /* consumer */);

  void OnStateChange() override;
  String DebugName() const override { return "BytesConsumerTestReader"; }
  std::pair<BytesConsumer::Result, Vector<char>> Run();
  std::pair<BytesConsumer::Result, Vector<char>> Run(
      scheduler::FakeTaskRunner*);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  Member<BytesConsumer> consumer_;
  BytesConsumer::Result result_ = BytesConsumer::Result::kShouldWait;
  Vector<char> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_BYTES_CONSUMER_TEST_READER_H_
