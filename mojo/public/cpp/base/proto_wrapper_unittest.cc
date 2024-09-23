// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/proto_wrapper_mojom_traits.h"

#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "mojo/public/cpp/base/test.pb.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/proto_wrapper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace mojo_base {

TEST(ProtoWrapperTest, TraitsOk) {
  mojo_base::test::TestMessage in;

  in.set_test("This is a test");
  ASSERT_TRUE(in.IsInitialized());

  ProtoWrapper in_w(in);
  ASSERT_TRUE(in_w.is_valid());
  ProtoWrapper out_w;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::ProtoWrapper>(
          in_w, out_w));

  std::optional<mojo_base::test::TestMessage> out =
      out_w.As<mojo_base::test::TestMessage>();

  ASSERT_TRUE(out.has_value());
  ASSERT_EQ(in.test(), out.value().test());
}

TEST(ProtoWrapperTest, LargeMessage) {
  mojo_base::test::TestMessage in;

  std::string large(mojo_base::BigBuffer::kMaxInlineBytes * 2, 'x');

  in.set_test(large);
  ASSERT_TRUE(in.IsInitialized());
  ASSERT_GT(in.ByteSizeLong(), mojo_base::BigBuffer::kMaxInlineBytes);

  ProtoWrapper in_w(in);
  ASSERT_TRUE(in_w.is_valid());
  ProtoWrapper out_w;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::ProtoWrapper>(
          in_w, out_w));

  std::optional<mojo_base::test::TestMessage> out =
      out_w.As<mojo_base::test::TestMessage>();

  ASSERT_TRUE(out.has_value());
  ASSERT_EQ(in.test(), out.value().test());
}

TEST(ProtoWrapperTest, TraitsEquivilentMessages) {
  mojo_base::test::TestMessage in;

  in.set_test("This is a test");
  ASSERT_TRUE(in.IsInitialized());

  ProtoWrapper in_w(in);
  ASSERT_TRUE(in_w.is_valid());
  ProtoWrapper out_w;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::ProtoWrapper>(
          in_w, out_w));

  std::optional<mojo_base::test::TestMessage2> out =
      out_w.As<mojo_base::test::TestMessage2>();

  ASSERT_FALSE(out.has_value());
}

TEST(ProtoWrapperTest, TraitsDistinctMessages) {
  mojo_base::test::TestMessage in;

  in.set_test("This is a test");
  ASSERT_TRUE(in.IsInitialized());

  ProtoWrapper in_w(in);
  ASSERT_TRUE(in_w.is_valid());
  ProtoWrapper out_w;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::ProtoWrapper>(
          in_w, out_w));

  std::optional<mojo_base::test::TestMessage3> out =
      out_w.As<mojo_base::test::TestMessage3>();

  ASSERT_FALSE(out.has_value());
}

TEST(ProtoWrapperTest, ToFromBytes) {
  mojo_base::test::TestMessage in;

  in.set_test("This is a test");
  ASSERT_TRUE(in.IsInitialized());

  ProtoWrapper in_w(in);
  ASSERT_TRUE(in_w.is_valid());

  auto in_span = in_w.byte_span(ProtoWrapperBytes::GetPassKey()).value();
  ASSERT_GT(in_span.size(), 0u);
  ProtoWrapper out_w(in_span, "mojo_base.test.TestMessage2",
                     ProtoWrapperBytes::GetPassKey());
  ASSERT_TRUE(out_w.is_valid());

  auto out = out_w.As<mojo_base::test::TestMessage2>();

  ASSERT_TRUE(out.has_value());
  ASSERT_EQ(in.test(), out.value().test());
}

}  // namespace mojo_base
