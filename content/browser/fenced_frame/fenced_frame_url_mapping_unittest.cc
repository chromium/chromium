// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Validates the mapping contained in `pending_ad_components`.
//
// If `add_to_new_map` is false, `pending_ad_components` will be added to
// `fenced_frame_url_mapping` to mimic ShadowDOM behavior. Otherwise, they'll be
// added to a new FencedFrameURLMapping to mimic MPArch behavior, and
// `fenced_frame_url_mapping` is ignored.
//
// `expected_mapped_urls` contains the URLs the first URNs are expected to map
// to, and will be padded with "about:blank" URLs until it's
// blink::kMaxAdAuctionAdComponents in length.
void ValidatePendingAdComponentsMap(
    FencedFrameURLMapping* fenced_frame_url_mapping,
    bool add_to_new_map,
    const FencedFrameURLMapping::PendingAdComponentsMap& pending_ad_components,
    std::vector<GURL> expected_mapped_urls) {
  // Get URN array from `pending_ad_components` and validate the returned URN
  // array as much as possible prior to adding the URNs to
  // `fenced_frame_url_mapping`, to the extent that's possible. This needs to be
  // done before adding the URNs to `fenced_frame_url_mapping` so that this loop
  // can make sure the URNs don't exist in the mapping yet.
  std::vector<GURL> ad_component_urns = pending_ad_components.GetURNs();
  ASSERT_EQ(blink::kMaxAdAuctionAdComponents, ad_component_urns.size());
  for (size_t i = 0; i < ad_component_urns.size(); ++i) {
    // All entries in `ad_component_urns` should be distinct URNs.
    EXPECT_EQ(url::kUrnScheme, ad_component_urns[i].scheme_piece());
    for (size_t j = 0; j < i; ++j)
      EXPECT_NE(ad_component_urns[j], ad_component_urns[i]);

    // The URNs should not yet be in `fenced_frame_url_mapping`, so they can
    // safely be added to it.
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
        nested_ad_components;
    EXPECT_FALSE(fenced_frame_url_mapping->ConvertFencedFrameURNToURL(
        ad_component_urns[i], nested_ad_components));
    EXPECT_FALSE(nested_ad_components);
  }

  // Add the `pending_ad_components` to a mapping. If `add_to_new_map` is true,
  // use a new URL mapping.
  FencedFrameURLMapping new_frame_url_mapping;
  if (add_to_new_map)
    fenced_frame_url_mapping = &new_frame_url_mapping;
  pending_ad_components.ExportToMapping(*fenced_frame_url_mapping);

  // Now validate the changes made to `fenced_frame_url_mapping`.
  // PendingAdComponentsMap does not directly expose the URLs it provides, so
  // can only check the URLs in after the URN/URL pairs have been added to a
  // FencedFrameURLMapping.
  for (size_t i = 0; i < ad_component_urns.size(); ++i) {
    // The URNs should now be in `fenced_frame_url_mapping`. Look up the
    // corresponding URL, and make sure it's mapped to the correct URL.
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
        nested_pending_ad_components;
    absl::optional<GURL> mapped_url =
        fenced_frame_url_mapping->ConvertFencedFrameURNToURL(
            ad_component_urns[i], nested_pending_ad_components);
    ASSERT_TRUE(mapped_url);
    if (i < expected_mapped_urls.size()) {
      EXPECT_EQ(expected_mapped_urls[i], mapped_url);
    } else {
      EXPECT_EQ(GURL(url::kAboutBlankURL), mapped_url);
    }

    // Each added URN should also have a populated
    // `nested_pending_ad_components` structure, to prevent ads from knowing if
    // they were loaded in a fenced frame as an ad component or as the main ad.
    // Any information passed to ads violates the k-anonymity requirement.
    ASSERT_TRUE(nested_pending_ad_components);

    // If this it not an about:blank URL, then when loaded in a fenced frame, it
    // can recursively access its own nested ad components array, so recursively
    // check those as well.
    if (*mapped_url != GURL(url::kAboutBlankURL)) {
      // Nested URL maps map everything to "about:blank". They exist solely so
      // that top-level and nested component ads can't tell which one they are,
      // to prevent smuggling data based on whether an ad is loaded in a
      // top-level ad URL or a component ad URL.
      ValidatePendingAdComponentsMap(
          fenced_frame_url_mapping, add_to_new_map,
          *nested_pending_ad_components,
          /*expected_mapped_urls=*/std::vector<GURL>());
    }
  }
}

}  // namespace

TEST(FencedFrameURLMappingTest, AddAndConvert) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL test_url("https://foo.test");
  GURL urn_uuid = fenced_frame_url_mapping.AddFencedFrameURL(test_url);
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap> ad_components;
  EXPECT_EQ(test_url, fenced_frame_url_mapping
                          .ConvertFencedFrameURNToURL(urn_uuid, ad_components)
                          .value());
  EXPECT_EQ(absl::nullopt, ad_components);
}

TEST(FencedFrameURLMappingTest, NonExistentUUID) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL urn_uuid("urn:uuid:C36973B5E5D9DE59E4C4364F137B3C7A");
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap> ad_components;
  absl::optional<GURL> result =
      fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid,
                                                          ad_components);
  EXPECT_EQ(absl::nullopt, result);
  EXPECT_EQ(absl::nullopt, ad_components);
}

// Test the case `ad_component_urls` is empty. In this case, it should be filled
// with URNs that are mapped to about:blank.
TEST(FencedFrameURLMappingTest,
     AddFencedFrameURLWithInterestGroupAdComponentUrlsNoAdComponentsUrls) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  std::vector<GURL> ad_component_urls;

  GURL urn_uuid = fenced_frame_url_mapping
                      .AddFencedFrameURLWithInterestGroupAdComponentUrls(
                          top_level_url, ad_component_urls);
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components;
  EXPECT_EQ(top_level_url, fenced_frame_url_mapping.ConvertFencedFrameURNToURL(
                               urn_uuid, pending_ad_components));
  ASSERT_TRUE(pending_ad_components);

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *pending_ad_components,
                                 /*expected_mapped_urls=*/{});
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *pending_ad_components,
                                 /*expected_mapped_urls=*/{});
}

// Test the case `ad_component_urls` has a single URL.
TEST(FencedFrameURLMappingTest,
     AddFencedFrameURLWithInterestGroupAdComponentUrlsOneAdComponentUrl) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  std::vector<GURL> ad_component_urls{GURL("https://bar.test")};

  GURL urn_uuid = fenced_frame_url_mapping
                      .AddFencedFrameURLWithInterestGroupAdComponentUrls(
                          top_level_url, ad_component_urls);
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components;
  EXPECT_EQ(top_level_url, fenced_frame_url_mapping.ConvertFencedFrameURNToURL(
                               urn_uuid, pending_ad_components));
  ASSERT_TRUE(pending_ad_components);

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *pending_ad_components, ad_component_urls);
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *pending_ad_components, ad_component_urls);
}

// Test the case `ad_component_urls` has the maximum number of allowed ad
// component URLs.
TEST(FencedFrameURLMappingTest,
     AddFencedFrameURLWithInterestGroupAdComponentUrlsMaxAdComponentUrl) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  std::vector<GURL> ad_component_urls;
  for (size_t i = 0; i < blink::kMaxAdAuctionAdComponents; ++i) {
    ad_component_urls.emplace_back(
        GURL(base::StringPrintf("https://%zu.test/", i)));
  }

  GURL urn_uuid = fenced_frame_url_mapping
                      .AddFencedFrameURLWithInterestGroupAdComponentUrls(
                          top_level_url, ad_component_urls);
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components;
  EXPECT_EQ(top_level_url, fenced_frame_url_mapping.ConvertFencedFrameURNToURL(
                               urn_uuid, pending_ad_components));
  ASSERT_TRUE(pending_ad_components);

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *pending_ad_components, ad_component_urls);
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *pending_ad_components, ad_component_urls);
}

// Test the case `ad_component_urls` has the maximum number of allowed ad
// component URLs, and they're all identical. The main purpose of this test is
// to make sure they receive unique URNs, despite being identical URLs.
TEST(
    FencedFrameURLMappingTest,
    AddFencedFrameURLWithInterestGroupAdComponentUrlsMaxIdenticalAdComponentUrl) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  std::vector<GURL> ad_component_urls(blink::kMaxAdAuctionAdComponents,
                                      GURL("https://bar.test/"));

  GURL urn_uuid = fenced_frame_url_mapping
                      .AddFencedFrameURLWithInterestGroupAdComponentUrls(
                          top_level_url, ad_component_urls);
  absl::optional<FencedFrameURLMapping::PendingAdComponentsMap>
      pending_ad_components;
  EXPECT_EQ(top_level_url, fenced_frame_url_mapping.ConvertFencedFrameURNToURL(
                               urn_uuid, pending_ad_components));
  ASSERT_TRUE(pending_ad_components);

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *pending_ad_components,
                                 /*expected_mapped_urls=*/ad_component_urls);
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *pending_ad_components,
                                 /*expected_mapped_urls=*/ad_component_urls);
}

}  // namespace content
