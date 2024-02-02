// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_api_request_body_mojom_traits.h"

#include <tuple>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"

namespace blink {
namespace {

class FetchApiRequestBodyMojomTraitsTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FetchApiRequestBodyMojomTraitsTest, RoundTripEmpty) {
  ResourceRequestBody src;

  ResourceRequestBody dest;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              blink::mojom::blink::FetchAPIRequestBody>(src, dest));

  EXPECT_TRUE(dest.IsEmpty());
}

TEST_F(FetchApiRequestBodyMojomTraitsTest, RoundTripBytes) {
  ResourceRequestBody src(EncodedFormData::Create());
  src.FormBody()->AppendData("hello", 5);
  src.FormBody()->SetIdentifier(29);
  src.FormBody()->SetContainsPasswordData(true);

  ResourceRequestBody dest;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              blink::mojom::blink::FetchAPIRequestBody>(src, dest));

  ASSERT_TRUE(dest.FormBody());
  EXPECT_EQ(dest.FormBody()->Identifier(), 29);
  EXPECT_TRUE(dest.FormBody()->ContainsPasswordData());
  ASSERT_EQ(1u, dest.FormBody()->Elements().size());
  const FormDataElement& e = dest.FormBody()->Elements()[0];
  EXPECT_EQ(e.type_, FormDataElement::kData);
  EXPECT_EQ("hello", String(e.data_.data(), e.data_.size()));
}

TEST_F(FetchApiRequestBodyMojomTraitsTest, RoundTripFile) {
  ResourceRequestBody src(EncodedFormData::Create());
  const base::Time now = base::Time::Now();
  src.FormBody()->AppendFile("file.name", now);

  ResourceRequestBody dest;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              blink::mojom::blink::FetchAPIRequestBody>(src, dest));

  ASSERT_TRUE(dest.FormBody());
  ASSERT_EQ(1u, dest.FormBody()->Elements().size());
  const FormDataElement& e = dest.FormBody()->Elements()[0];
  EXPECT_EQ(e.type_, FormDataElement::kEncodedFile);
  EXPECT_EQ(e.filename_, "file.name");
  EXPECT_EQ(e.file_start_, 0);
  EXPECT_EQ(e.file_length_, BlobData::kToEndOfFile);
  EXPECT_EQ(e.expected_file_modification_time_, now);
}

TEST_F(FetchApiRequestBodyMojomTraitsTest, RoundTripFileRange) {
  ResourceRequestBody src(EncodedFormData::Create());
  src.FormBody()->AppendFileRange("abc", 4, 8, std::nullopt);

  ResourceRequestBody dest;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              blink::mojom::blink::FetchAPIRequestBody>(src, dest));

  ASSERT_TRUE(dest.FormBody());
  ASSERT_EQ(1u, dest.FormBody()->Elements().size());
  const FormDataElement& e = dest.FormBody()->Elements()[0];
  EXPECT_EQ(e.type_, FormDataElement::kEncodedFile);
  EXPECT_EQ(e.filename_, "abc");
  EXPECT_EQ(e.file_start_, 4);
  EXPECT_EQ(e.file_length_, 8);
  EXPECT_EQ(e.expected_file_modification_time_, std::nullopt);
}

TEST_F(FetchApiRequestBodyMojomTraitsTest, RoundTripBlobWithOpionalHandle) {
  ResourceRequestBody src(EncodedFormData::Create());
  mojo::MessagePipe pipe;
  String uuid = "test_uuid";
  auto blob_data_handle = BlobDataHandle::Create(
      uuid, "type-test", 100,
      mojo::PendingRemote<mojom::blink::Blob>(std::move(pipe.handle0), 0));
  src.FormBody()->AppendBlob(uuid, blob_data_handle);

  ResourceRequestBody dest;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              blink::mojom::blink::FetchAPIRequestBody>(src, dest));

  ASSERT_TRUE(dest.FormBody());
  ASSERT_EQ(1u, dest.FormBody()->Elements().size());
  const FormDataElement& e = dest.FormBody()->Elements()[0];
  EXPECT_EQ(e.type_, FormDataElement::kDataPipe);
  EXPECT_EQ(e.blob_uuid_, String());
  EXPECT_TRUE(e.data_pipe_getter_);
}

TEST_F(FetchApiRequestBodyMojomTraitsTest, RoundTripDataPipeGetter) {
  ResourceRequestBody src(EncodedFormData::Create());
  mojo::PendingRemote<network::mojom::blink::DataPipeGetter> data_pipe_getter;
  std::ignore = data_pipe_getter.InitWithNewPipeAndPassReceiver();
  src.FormBody()->AppendDataPipe(
      base::MakeRefCounted<blink::WrappedDataPipeGetter>(
          std::move(data_pipe_getter)));

  ResourceRequestBody dest;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              blink::mojom::blink::FetchAPIRequestBody>(src, dest));

  ASSERT_TRUE(dest.FormBody());
  ASSERT_EQ(1u, dest.FormBody()->Elements().size());
  const FormDataElement& e = dest.FormBody()->Elements()[0];
  EXPECT_EQ(e.type_, FormDataElement::kDataPipe);
  EXPECT_TRUE(e.data_pipe_getter_);
}

TEST_F(FetchApiRequestBodyMojomTraitsTest, RoundTripStreamBody) {
  mojo::PendingRemote<network::mojom::blink::ChunkedDataPipeGetter>
      chunked_data_pipe_getter;
  std::ignore = chunked_data_pipe_getter.InitWithNewPipeAndPassReceiver();
  ResourceRequestBody src(std::move(chunked_data_pipe_getter));

  ResourceRequestBody dest;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              blink::mojom::blink::FetchAPIRequestBody>(src, dest));

  EXPECT_FALSE(dest.FormBody());
  ASSERT_TRUE(dest.StreamBody());
}

}  // namespace
}  // namespace blink
