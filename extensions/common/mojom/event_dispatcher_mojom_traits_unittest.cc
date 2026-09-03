// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/event_dispatcher_mojom_traits.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(EventArgsTraitsTest, SerializeAndDeserialize) {
  base::ListValue original_list;
  original_list.Append("hello");
  original_list.Append(42);
  scoped_refptr<const EventArgs> input =
      base::MakeRefCounted<EventArgs>(std::move(original_list));

  scoped_refptr<const EventArgs> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::EventArgs>(input, output));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->data, input->data);
  EXPECT_EQ(output->data.size(), 2u);
  EXPECT_EQ(output->data[0].GetString(), "hello");
  EXPECT_EQ(output->data[1].GetInt(), 42);
}

TEST(EventArgsTraitsTest, SerializeEmptyList) {
  scoped_refptr<const EventArgs> input = base::MakeRefCounted<EventArgs>();

  scoped_refptr<const EventArgs> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::EventArgs>(input, output));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->data.empty());
}

}  // namespace extensions
