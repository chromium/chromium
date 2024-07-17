// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(GrowableIOBufferTest, SpanBeforeOffset) {
  scoped_refptr<net::GrowableIOBuffer> buffer =
      base::MakeRefCounted<net::GrowableIOBuffer>();
  buffer->SetCapacity(100);
  EXPECT_EQ(0u, buffer->span_before_offset().size());

  buffer->set_offset(10);
  EXPECT_EQ(10u, buffer->span_before_offset().size());
  EXPECT_EQ(buffer->everything().data(), buffer->span_before_offset().data());

  buffer->set_offset(100);
  EXPECT_EQ(100u, buffer->span_before_offset().size());
  EXPECT_EQ(buffer->everything().data(), buffer->span_before_offset().data());
}

}  // anonymous namespace

}  // namespace net
