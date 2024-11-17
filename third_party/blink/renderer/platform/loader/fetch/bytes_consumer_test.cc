// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(BytesConusmerTest, ClosedBytesConsumer) {
  BytesConsumer* consumer = BytesConsumer::CreateClosed();

  base::span<const char> buffer;
  EXPECT_EQ(BytesConsumer::Result::kDone, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kClosed, consumer->GetPublicState());
}

TEST(BytesConusmerTest, ErroredBytesConsumer) {
  BytesConsumer::Error error("hello");
  BytesConsumer* consumer = BytesConsumer::CreateErrored(error);

  base::span<const char> buffer;
  EXPECT_EQ(BytesConsumer::Result::kError, consumer->BeginRead(buffer));
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(error.Message(), consumer->GetError().Message());

  consumer->Cancel();
  EXPECT_EQ(BytesConsumer::PublicState::kErrored, consumer->GetPublicState());
}

}  // namespace

}  // namespace blink
