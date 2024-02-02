// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"

#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"

namespace blink {

class AlternateSignedExchangeResourceInfoTest : public testing::Test {
 public:
  AlternateSignedExchangeResourceInfoTest() = default;
  AlternateSignedExchangeResourceInfoTest(
      const AlternateSignedExchangeResourceInfoTest&) = delete;
  AlternateSignedExchangeResourceInfoTest& operator=(
      const AlternateSignedExchangeResourceInfoTest&) = delete;
  ~AlternateSignedExchangeResourceInfoTest() override = default;

 protected:
  const AlternateSignedExchangeResourceInfo::EntryMap& GetEntries(
      const AlternateSignedExchangeResourceInfo* info) {
    return info->alternative_resources_;
  }
};

TEST_F(AlternateSignedExchangeResourceInfoTest, Empty) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid("", "");
  EXPECT_FALSE(info);
}

TEST_F(AlternateSignedExchangeResourceInfoTest, Simple) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid(
          // Outer link header
          "<https://distributor.example/publisher.example/script.js.sxg>;"
          "rel=\"alternate\";"
          "type=\"application/signed-exchange;v=b3\";"
          "anchor=\"https://publisher.example/script.js\"",
          // Inner link header
          "<https://publisher.example/script.js>;"
          "rel=\"allowed-alt-sxg\";"
          "header-integrity="
          "\"sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=\"");
  ASSERT_TRUE(info);
  const auto& entries = GetEntries(info.get());
  ASSERT_EQ(1u, entries.size());
  const auto& it = entries.find(KURL("https://publisher.example/script.js"));
  ASSERT_TRUE(it != entries.end());
  ASSERT_EQ(1u, it->value.size());
  const auto& resource = it->value.at(0);
  EXPECT_EQ(KURL("https://publisher.example/script.js"),
            resource->anchor_url());
  EXPECT_EQ(KURL("https://distributor.example/publisher.example/script.js.sxg"),
            resource->alternative_url());
  EXPECT_EQ("sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=",
            resource->header_integrity());
  EXPECT_TRUE(resource->variants().empty());
  EXPECT_TRUE(resource->variant_key().empty());

  EXPECT_EQ(resource.get(),
            info->FindMatchingEntry(KURL("https://publisher.example/script.js"),
                                    std::nullopt, {"en"}));
  EXPECT_EQ(nullptr,
            info->FindMatchingEntry(KURL("https://publisher.example/image"),
                                    std::nullopt, {"en"}));
}

TEST_F(AlternateSignedExchangeResourceInfoTest, MultipleResources) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid(
          // The first outer link header
          "<https://distributor.example/publisher.example/script.js.sxg>;"
          "rel=\"alternate\";"
          "type=\"application/signed-exchange;v=b3\";"
          "anchor=\"https://publisher.example/script.js\","
          // The second outer_link_header
          "<https://distributor.example/publisher.example/image.sxg>;"
          "rel=\"alternate\";"
          "type=\"application/signed-exchange;v=b3\";"
          "anchor=\"https://publisher.example/image\";",
          // The first inner link header
          "<https://publisher.example/script.js>;"
          "rel=\"allowed-alt-sxg\";"
          "header-integrity="
          "\"sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=\","
          // The second inner link header
          "<https://publisher.example/image>;"
          "rel=\"allowed-alt-sxg\";"
          "header-integrity="
          "\"sha256-q1phjFcR+umcl0zBaEz6E5AGVlnc9yF0zOjDYi5c6aM=\"");
  ASSERT_TRUE(info);
  const auto& entries = GetEntries(info.get());
  ASSERT_EQ(2u, entries.size());
  {
    const auto& it = entries.find(KURL("https://publisher.example/script.js"));
    ASSERT_TRUE(it != entries.end());
    ASSERT_EQ(1u, it->value.size());
    const auto& resource = it->value.at(0);
    EXPECT_EQ(KURL("https://publisher.example/script.js"),
              resource->anchor_url());
    EXPECT_EQ(
        KURL("https://distributor.example/publisher.example/script.js.sxg"),
        resource->alternative_url());
    EXPECT_EQ("sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=",
              resource->header_integrity());
    EXPECT_TRUE(resource->variants().empty());
    EXPECT_TRUE(resource->variant_key().empty());
    EXPECT_EQ(resource.get(), info->FindMatchingEntry(
                                  KURL("https://publisher.example/script.js"),
                                  std::nullopt, {"en"}));
  }
  {
    const auto& it = entries.find(KURL("https://publisher.example/image"));
    ASSERT_TRUE(it != entries.end());
    ASSERT_EQ(1u, it->value.size());
    const auto& resource = it->value.at(0);
    EXPECT_EQ(KURL("https://publisher.example/image"), resource->anchor_url());
    EXPECT_EQ(KURL("https://distributor.example/publisher.example/image.sxg"),
              resource->alternative_url());
    EXPECT_EQ("sha256-q1phjFcR+umcl0zBaEz6E5AGVlnc9yF0zOjDYi5c6aM=",
              resource->header_integrity());
    EXPECT_TRUE(resource->variants().empty());
    EXPECT_TRUE(resource->variant_key().empty());
    EXPECT_EQ(resource.get(),
              info->FindMatchingEntry(KURL("https://publisher.example/image"),
                                      std::nullopt, {"en"}));
  }
}

TEST_F(AlternateSignedExchangeResourceInfoTest,
       NoMatchingOuterAlternateLinkHeader) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid(
          // Empty outer link header
          "",
          // Inner link header
          "<https://publisher.example/script.js>;"
          "rel=\"allowed-alt-sxg\";"
          "header-integrity="
          "\"sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=\"");
  ASSERT_TRUE(info);
  const auto& entries = GetEntries(info.get());
  ASSERT_EQ(1u, entries.size());
  const auto& it = entries.find(KURL("https://publisher.example/script.js"));
  ASSERT_TRUE(it != entries.end());
  ASSERT_EQ(1u, it->value.size());
  const auto& resource = it->value.at(0);
  EXPECT_EQ(KURL("https://publisher.example/script.js"),
            resource->anchor_url());
  EXPECT_FALSE(resource->alternative_url().IsValid());
  EXPECT_EQ("sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=",
            resource->header_integrity());
  EXPECT_TRUE(resource->variants().empty());
  EXPECT_TRUE(resource->variant_key().empty());

  EXPECT_EQ(resource.get(),
            info->FindMatchingEntry(KURL("https://publisher.example/script.js"),
                                    std::nullopt, {"en"}));
}

TEST_F(AlternateSignedExchangeResourceInfoTest, NoType) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid(
          // Outer link header
          "<https://distributor.example/publisher.example/script.js.sxg>;"
          "rel=\"alternate\";"
          "anchor=\"https://publisher.example/script.js\"",
          // Inner link header
          "<https://publisher.example/script.js>;"
          "rel=\"allowed-alt-sxg\";"
          "header-integrity="
          "\"sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=\"");
  ASSERT_TRUE(info);
  const auto& entries = GetEntries(info.get());
  ASSERT_EQ(1u, entries.size());
  const auto& it = entries.find(KURL("https://publisher.example/script.js"));
  ASSERT_TRUE(it != entries.end());
  ASSERT_EQ(1u, it->value.size());
  const auto& resource = it->value.at(0);
  EXPECT_EQ(KURL("https://publisher.example/script.js"),
            resource->anchor_url());
  // If type is not "application/signed-exchange;v=b3", outer alternate link
  // header is ignored.
  EXPECT_FALSE(resource->alternative_url().IsValid());
  EXPECT_EQ("sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=",
            resource->header_integrity());
  EXPECT_TRUE(resource->variants().empty());
  EXPECT_TRUE(resource->variant_key().empty());

  EXPECT_EQ(resource.get(),
            info->FindMatchingEntry(KURL("https://publisher.example/script.js"),
                                    std::nullopt, {"en"}));
  EXPECT_EQ(nullptr,
            info->FindMatchingEntry(KURL("https://publisher.example/image"),
                                    std::nullopt, {"en"}));
}

TEST_F(AlternateSignedExchangeResourceInfoTest, InvalidOuterURL) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid(
          // Outer link header: Outer URL is invalid.
          "<INVALID_OUTER_URL>;"
          "rel=\"alternate\";"
          "type=\"application/signed-exchange;v=b3\";"
          "anchor=\"https://publisher.example/script.js\"",
          // Inner link header
          "<https://publisher.example/script.js>;"
          "rel=\"allowed-alt-sxg\";"
          "header-integrity="
          "\"sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=\"");
  ASSERT_TRUE(info);
  const auto& entries = GetEntries(info.get());
  ASSERT_EQ(1u, entries.size());
  const auto& it = entries.find(KURL("https://publisher.example/script.js"));
  ASSERT_TRUE(it != entries.end());
  ASSERT_EQ(1u, it->value.size());
  const auto& resource = it->value.at(0);
  EXPECT_EQ(KURL("https://publisher.example/script.js"),
            resource->anchor_url());
  EXPECT_FALSE(resource->alternative_url().IsValid());
  EXPECT_EQ("sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=",
            resource->header_integrity());
  EXPECT_TRUE(resource->variants().empty());
  EXPECT_TRUE(resource->variant_key().empty());

  EXPECT_EQ(resource.get(),
            info->FindMatchingEntry(KURL("https://publisher.example/script.js"),
                                    std::nullopt, {"en"}));
}

TEST_F(AlternateSignedExchangeResourceInfoTest, InvalidInnerURL) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid(
          // Outer link header: Inner URL is invalid.
          "<https://distributor.example/publisher.example/script.js.sxg>;"
          "rel=\"alternate\";"
          "type=\"application/signed-exchange;v=b3\";"
          "anchor=\"INVALID_INNER_URL\"",
          // Inner link header: Inner URL is invalid.
          "<INVALID_INNER_URL>;"
          "rel=\"allowed-alt-sxg\";"
          "header-integrity="
          "\"sha256-7KheEN4nyNxE3c4yQZdgCBJthJ2UwgpLSBeSUpII+jg=\"");
  ASSERT_FALSE(info);
}

TEST_F(AlternateSignedExchangeResourceInfoTest, Variants) {
  std::unique_ptr<AlternateSignedExchangeResourceInfo> info =
      AlternateSignedExchangeResourceInfo::CreateIfValid(
          // The first outer link header
          "<https://distributor.example/publisher.example/image_jpeg.sxg>;"
          "rel=\"alternate\";"
          "type=\"application/signed-exchange;v=b3\";"
          "variants-04=\"accept;image/jpeg;image/webp\";"
          "variant-key-04=\"image/jpeg\";"
          "anchor=\"https://publisher.example/image\";,"
          // The second outer link header
          "<https://distributor.example/publisher.example/image_webp.sxg>;"
          "rel=\"alternate\";"
          "type=\"application/signed-exchange;v=b3\";"
          "variants-04=\"accept;image/jpeg;image/webp\";"
          "variant-key-04=\"image/webp\";"
          "anchor=\"https://publisher.example/image\"",
          // The first inner link header
          "<https://publisher.example/image>;"
          "rel=\"allowed-alt-sxg\";"
          "variants-04=\"accept;image/jpeg;image/webp\";"
          "variant-key-04=\"image/jpeg\";"
          "header-integrity="
          "\"sha256-q1phjFcR+umcl0zBaEz6E5AGVlnc9yF0zOjDYi5c6aM=\","
          // The second inner link header
          "<https://publisher.example/image>;"
          "rel=\"allowed-alt-sxg\";"
          "variants-04=\"accept;image/jpeg;image/webp\";"
          "variant-key-04=\"image/webp\";"
          "header-integrity="
          "\"sha256-KRcYU+BZK8Sb2ccJfDPz+uUKXDdB1PVToPugItdzRXY=\"");
  ASSERT_TRUE(info);
  const auto& entries = GetEntries(info.get());
  ASSERT_EQ(1u, entries.size());
  const auto& it = entries.find(KURL("https://publisher.example/image"));
  ASSERT_TRUE(it != entries.end());
  ASSERT_EQ(2u, it->value.size());
  {
    const auto& resource = it->value.at(0);
    EXPECT_EQ(KURL("https://publisher.example/image"), resource->anchor_url());
    EXPECT_EQ(
        KURL("https://distributor.example/publisher.example/image_jpeg.sxg"),
        resource->alternative_url());
    EXPECT_EQ("sha256-q1phjFcR+umcl0zBaEz6E5AGVlnc9yF0zOjDYi5c6aM=",
              resource->header_integrity());
    EXPECT_EQ("accept;image/jpeg;image/webp", resource->variants());
    EXPECT_EQ("image/jpeg", resource->variant_key());
  }
  {
    const auto& resource = it->value.at(1);
    EXPECT_EQ(KURL("https://publisher.example/image"), resource->anchor_url());
    EXPECT_EQ(
        KURL("https://distributor.example/publisher.example/image_webp.sxg"),
        resource->alternative_url());
    EXPECT_EQ("sha256-KRcYU+BZK8Sb2ccJfDPz+uUKXDdB1PVToPugItdzRXY=",
              resource->header_integrity());
    EXPECT_EQ("accept;image/jpeg;image/webp", resource->variants());
    EXPECT_EQ("image/webp", resource->variant_key());

    EXPECT_EQ(resource.get(),
              info->FindMatchingEntry(
                  KURL("https://publisher.example/image"),
                  network::mojom::RequestDestination::kImage, {"en"}));
  }
}

}  // namespace blink
