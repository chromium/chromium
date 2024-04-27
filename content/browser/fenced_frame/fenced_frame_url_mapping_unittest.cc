// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/fenced_frame_test_utils.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Validates the mapping contained in `pending_ad_components`.
//
// If `add_to_new_map` is false, `pending_ad_components` will be added to
// `fenced_frame_url_mapping` to mimic ShadowDOM behavior. Otherwise, they'll
// be added to a new FencedFrameURLMapping to mimic MPArch behavior, and
// `fenced_frame_url_mapping` is ignored.
//
// `expected_mapped_ad_descriptors` contains the URLs the first URNs are
// expected to map to, and will be padded with "about:blank" URLs until it's
// blink::MaxAdAuctionAdComponents() in length.
// TODO(crbug.com/40202462): the ShadowDOM implementation is deprecated, and
// these tests should be cleaned up to only reflect MPArch behavior.
void ValidatePendingAdComponentsMap(
    FencedFrameURLMapping* fenced_frame_url_mapping,
    bool add_to_new_map,
    const std::vector<std::pair<GURL, FencedFrameConfig>>&
        nested_urn_config_pairs,
    std::vector<blink::AdDescriptor> expected_mapped_ad_descriptors) {
  // Get URN array from `nested_urn_config_pairs` and validate the returned
  // URN array as much as possible prior to adding the URNs to
  // `fenced_frame_url_mapping`, to the extent that's possible. This needs to
  // be done before adding the URNs to `fenced_frame_url_mapping` so that this
  // loop can make sure the URNs don't exist in the mapping yet.
  std::vector<GURL> ad_component_urns;
  for (auto& urn_config_pair : nested_urn_config_pairs) {
    ad_component_urns.push_back(urn_config_pair.first);
  }
  ASSERT_EQ(blink::MaxAdAuctionAdComponents(), ad_component_urns.size());
  for (size_t i = 0; i < ad_component_urns.size(); ++i) {
    // All entries in `ad_component_urns` should be distinct URNs.
    EXPECT_EQ(url::kUrnScheme, ad_component_urns[i].scheme_piece());
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(ad_component_urns[j], ad_component_urns[i]);
    }

    // The URNs should not yet be in `fenced_frame_url_mapping`, so they can
    // safely be added to it.
    TestFencedFrameURLMappingResultObserver observer;
    fenced_frame_url_mapping->ConvertFencedFrameURNToURL(ad_component_urns[i],
                                                         &observer);
    EXPECT_TRUE(observer.mapping_complete_observed());
    EXPECT_FALSE(observer.mapped_url());
    EXPECT_FALSE(observer.nested_urn_config_pairs());
  }

  // Add the `nested_urn_config_pairs` to a mapping. If `add_to_new_map` is
  // true, use a new URL mapping.
  FencedFrameURLMapping new_frame_url_mapping;
  if (add_to_new_map) {
    fenced_frame_url_mapping = &new_frame_url_mapping;
  }
  fenced_frame_url_mapping->ImportPendingAdComponents(nested_urn_config_pairs);

  // Now validate the changes made to `fenced_frame_url_mapping`.
  for (size_t i = 0; i < ad_component_urns.size(); ++i) {
    // The URNs should now be in `fenced_frame_url_mapping`. Look up the
    // corresponding URL, and make sure it's mapped to the correct URL.
    TestFencedFrameURLMappingResultObserver observer;
    fenced_frame_url_mapping->ConvertFencedFrameURNToURL(ad_component_urns[i],
                                                         &observer);
    EXPECT_TRUE(observer.mapping_complete_observed());

    if (i < expected_mapped_ad_descriptors.size()) {
      EXPECT_EQ(expected_mapped_ad_descriptors[i].url, observer.mapped_url());
    } else {
      EXPECT_EQ(GURL(url::kAboutBlankURL), observer.mapped_url());
    }

    // Each added URN should also have a populated
    // `observer.nested_urn_config_pairs()` structure, to prevent ads from
    // knowing if they were loaded in a fenced frame as an ad component or as
    // the main ad. Any information passed to ads violates the k-anonymity
    // requirement.
    EXPECT_TRUE(observer.nested_urn_config_pairs());

    // If this it not an about:blank URL, then when loaded in a fenced frame,
    // it can recursively access its own nested ad components array, so
    // recursively check those as well.
    if (*observer.mapped_url() != GURL(url::kAboutBlankURL)) {
      // Nested URL maps map everything to "about:blank". They exist solely so
      // that top-level and nested component ads can't tell which one they
      // are, to prevent smuggling data based on whether an ad is loaded in a
      // top-level ad URL or a component ad URL.
      ValidatePendingAdComponentsMap(fenced_frame_url_mapping, add_to_new_map,
                                     *observer.nested_urn_config_pairs(),
                                     /*expected_mapped_ad_descriptors=*/
                                     {});
    }
  }
}

GURL GenerateAndVerifyPendingMappedURN(
    FencedFrameURLMapping* fenced_frame_url_mapping) {
  std::optional<GURL> pending_urn =
      fenced_frame_url_mapping->GeneratePendingMappedURN();
  EXPECT_TRUE(pending_urn.has_value());
  EXPECT_TRUE(pending_urn->is_valid());

  return pending_urn.value();
}

class FencedFrameURLMappingTest : public RenderViewHostTestHarness {
 public:
  FencedFrameURLMappingTest() = default;

  // Creates a dummy FencedFrameReporter that will never used to send any
  // reports. Tests only check for pointer equality, so the configuration of the
  // FencedFrameReporter does not matter.
  scoped_refptr<FencedFrameReporter> CreateReporter() {
    return FencedFrameReporter::CreateForFledge(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            nullptr),
        /*browser_context=*/browser_context(),
        /*direct_seller_is_seller=*/false,
        /*private_aggregation_manager=*/nullptr,
        /*main_frame_origin=*/url::Origin(),
        /*winner_origin=*/url::Origin(),
        /*winner_aggregation_coordinator_origin=*/std::nullopt);
  }
};

}  // namespace

TEST_F(FencedFrameURLMappingTest, AddAndConvert) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL test_url("https://foo.test");
  std::optional<GURL> urn_uuid =
      fenced_frame_url_mapping.AddFencedFrameURLForTesting(test_url);
  EXPECT_TRUE(urn_uuid.has_value());

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid.value(),
                                                      &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(test_url, observer.mapped_url());
  EXPECT_EQ(std::nullopt, observer.nested_urn_config_pairs());
}

TEST_F(FencedFrameURLMappingTest, NonExistentUUID) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL urn_uuid("urn:uuid:c36973b5-e5d9-de59-e4c4-364f137b3c7a");

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(std::nullopt, observer.mapped_url());
  EXPECT_EQ(std::nullopt, observer.nested_urn_config_pairs());
}

TEST_F(FencedFrameURLMappingTest, PendingMappedUUID) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  const GURL urn_uuid1 =
      GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);
  const GURL urn_uuid2 =
      GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  TestFencedFrameURLMappingResultObserver observer1;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid1, &observer1);
  EXPECT_FALSE(observer1.mapping_complete_observed());

  TestFencedFrameURLMappingResultObserver observer2;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid2, &observer2);
  EXPECT_FALSE(observer2.mapping_complete_observed());

  net::SchemefulSite shared_storage_site(GURL("https://bar.com"));
  GURL mapped_url = GURL("https://foo.com");

  // Two SharedStorageBudgetMetadata for the same site can happen if the same
  // blink::Document invokes window.sharedStorage.runURLSelectionOperation()
  // twice. Each call will generate a distinct URN. And if the input urls have
  // different size, the budget_to_charge (i.e. log(n)) will be also different.
  SimulateSharedStorageURNMappingComplete(fenced_frame_url_mapping, urn_uuid1,
                                          mapped_url, shared_storage_site,
                                          /*budget_to_charge=*/2.0);

  SimulateSharedStorageURNMappingComplete(fenced_frame_url_mapping, urn_uuid2,
                                          mapped_url, shared_storage_site,
                                          /*budget_to_charge=*/3.0);

  EXPECT_TRUE(observer1.mapping_complete_observed());
  EXPECT_EQ(mapped_url, observer1.mapped_url());
  EXPECT_EQ(std::nullopt, observer1.nested_urn_config_pairs());

  EXPECT_TRUE(observer2.mapping_complete_observed());
  EXPECT_EQ(mapped_url, observer2.mapped_url());
  EXPECT_EQ(std::nullopt, observer2.nested_urn_config_pairs());

  SharedStorageBudgetMetadata* metadata1 =
      fenced_frame_url_mapping.GetSharedStorageBudgetMetadataForTesting(
          urn_uuid1);

  EXPECT_TRUE(metadata1);
  EXPECT_EQ(metadata1->site, shared_storage_site);
  EXPECT_DOUBLE_EQ(metadata1->budget_to_charge, 2.0);

  SharedStorageBudgetMetadata* metadata2 =
      fenced_frame_url_mapping.GetSharedStorageBudgetMetadataForTesting(
          urn_uuid2);

  EXPECT_TRUE(metadata2);
  EXPECT_EQ(metadata2->site, shared_storage_site);
  EXPECT_DOUBLE_EQ(metadata2->budget_to_charge, 3.0);
}

TEST_F(FencedFrameURLMappingTest, RemoveObserverOnPendingMappedUUID) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  const GURL urn_uuid =
      GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_FALSE(observer.mapping_complete_observed());

  fenced_frame_url_mapping.RemoveObserverForURN(urn_uuid, &observer);

  SimulateSharedStorageURNMappingComplete(
      fenced_frame_url_mapping, urn_uuid,
      /*mapped_url=*/GURL("https://foo.com"),
      /*shared_storage_site=*/
      net::SchemefulSite::Deserialize("https://bar.com"),
      /*budget_to_charge=*/2.0);

  EXPECT_FALSE(observer.mapping_complete_observed());
}

TEST_F(FencedFrameURLMappingTest, RegisterTwoObservers) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  const GURL urn_uuid =
      GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  TestFencedFrameURLMappingResultObserver observer1;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer1);
  EXPECT_FALSE(observer1.mapping_complete_observed());

  TestFencedFrameURLMappingResultObserver observer2;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer2);
  EXPECT_FALSE(observer2.mapping_complete_observed());

  SimulateSharedStorageURNMappingComplete(
      fenced_frame_url_mapping, urn_uuid,
      /*mapped_url=*/GURL("https://foo.com"),
      /*shared_storage_site=*/
      net::SchemefulSite::Deserialize("https://bar.com"),
      /*budget_to_charge=*/2.0);

  EXPECT_TRUE(observer1.mapping_complete_observed());
  EXPECT_EQ(GURL("https://foo.com"), observer1.mapped_url());
  EXPECT_EQ(std::nullopt, observer1.nested_urn_config_pairs());
  EXPECT_TRUE(observer2.mapping_complete_observed());
  EXPECT_EQ(GURL("https://foo.com"), observer2.mapped_url());
  EXPECT_EQ(std::nullopt, observer2.nested_urn_config_pairs());
}

// Test the case `ad_component_descriptors` is empty. In this case, it should
// be filled with URNs that are mapped to about:blank.
TEST_F(FencedFrameURLMappingTest,
       AssignFencedFrameURLAndInterestGroupInfoNoAdComponentsUrls) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  url::Origin interest_group_owner = url::Origin::Create(top_level_url);
  std::string interest_group_name = "bars";
  std::vector<blink::AdDescriptor> ad_component_descriptors;

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  bool on_navigate_callback_invoked = false;
  base::RepeatingCallback on_navigate_callback =
      base::BindLambdaForTesting([&on_navigate_callback_invoked]() {
        EXPECT_FALSE(on_navigate_callback_invoked);
        on_navigate_callback_invoked = true;
      });
  fenced_frame_url_mapping.AssignFencedFrameURLAndInterestGroupInfo(
      urn_uuid, /*container_size=*/std::nullopt,
      blink::AdDescriptor(top_level_url),
      {interest_group_owner, interest_group_name}, on_navigate_callback,
      ad_component_descriptors);

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(top_level_url, observer.mapped_url());
  EXPECT_EQ(interest_group_owner,
            observer.ad_auction_data()->interest_group_owner);
  EXPECT_EQ(interest_group_name,
            observer.ad_auction_data()->interest_group_name);
  EXPECT_TRUE(observer.nested_urn_config_pairs());
  ASSERT_TRUE(observer.on_navigate_callback());
  EXPECT_FALSE(on_navigate_callback_invoked);
  observer.on_navigate_callback().Run();
  EXPECT_TRUE(on_navigate_callback_invoked);

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *observer.nested_urn_config_pairs(),
                                 /*expected_mapped_ad_descriptors=*/{});
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *observer.nested_urn_config_pairs(),
                                 /*expected_mapped_ad_descriptors=*/{});
}

// Test the case `ad_component_descriptors` has a single URL.
TEST_F(FencedFrameURLMappingTest,
       AssignFencedFrameURLAndInterestGroupInfoOneAdComponentUrl) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  url::Origin interest_group_owner = url::Origin::Create(top_level_url);
  std::string interest_group_name = "bars";
  std::vector<blink::AdDescriptor> ad_component_descriptors{
      blink::AdDescriptor(GURL("https://bar.test"))};

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  fenced_frame_url_mapping.AssignFencedFrameURLAndInterestGroupInfo(
      urn_uuid, /*container_size=*/std::nullopt,
      blink::AdDescriptor(top_level_url),
      {interest_group_owner, interest_group_name},
      /*on_navigate_callback=*/base::RepeatingClosure(),
      ad_component_descriptors);

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(top_level_url, observer.mapped_url());
  EXPECT_EQ(interest_group_owner,
            observer.ad_auction_data()->interest_group_owner);
  EXPECT_EQ(interest_group_name,
            observer.ad_auction_data()->interest_group_name);
  EXPECT_TRUE(observer.nested_urn_config_pairs());
  EXPECT_FALSE(observer.on_navigate_callback());

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *observer.nested_urn_config_pairs(),
                                 ad_component_descriptors);
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *observer.nested_urn_config_pairs(),
                                 ad_component_descriptors);
}

// Test the case `ad_component_descriptors` has the maximum number of allowed
// ad component URLs.
TEST_F(FencedFrameURLMappingTest,
       AssignFencedFrameURLAndInterestGroupInfoMaxAdComponentUrl) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  url::Origin interest_group_owner = url::Origin::Create(top_level_url);
  std::string interest_group_name = "bars";
  std::vector<blink::AdDescriptor> ad_component_descriptors;
  const size_t kMaxAdAuctionAdComponents = blink::MaxAdAuctionAdComponents();
  for (size_t i = 0; i < kMaxAdAuctionAdComponents; ++i) {
    ad_component_descriptors.emplace_back(
        GURL(base::StringPrintf("https://%zu.test/", i)));
  }

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  fenced_frame_url_mapping.AssignFencedFrameURLAndInterestGroupInfo(
      urn_uuid, /*container_size=*/std::nullopt,
      blink::AdDescriptor(top_level_url),
      {interest_group_owner, interest_group_name},
      /*on_navigate_callback=*/base::RepeatingClosure(),
      ad_component_descriptors);

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(top_level_url, observer.mapped_url());
  EXPECT_EQ(interest_group_owner,
            observer.ad_auction_data()->interest_group_owner);
  EXPECT_EQ(interest_group_name,
            observer.ad_auction_data()->interest_group_name);
  EXPECT_TRUE(observer.nested_urn_config_pairs());
  EXPECT_FALSE(observer.on_navigate_callback());

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *observer.nested_urn_config_pairs(),
                                 ad_component_descriptors);
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *observer.nested_urn_config_pairs(),
                                 ad_component_descriptors);
}

// Test the case `ad_component_descriptors` has the maximum number of allowed
// ad component URLs, and they're all identical. The main purpose of this test
// is to make sure they receive unique URNs, despite being identical URLs.
TEST_F(FencedFrameURLMappingTest,
       AssignFencedFrameURLAndInterestGroupInfoMaxIdenticalAdComponentUrl) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url("https://foo.test");
  url::Origin interest_group_owner = url::Origin::Create(top_level_url);
  std::string interest_group_name = "bars";
  std::vector<blink::AdDescriptor> ad_component_descriptors(
      blink::MaxAdAuctionAdComponents(),
      blink::AdDescriptor(GURL("https://bar.test/")));

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  fenced_frame_url_mapping.AssignFencedFrameURLAndInterestGroupInfo(
      urn_uuid, /*container_size=*/std::nullopt,
      blink::AdDescriptor(top_level_url),
      {interest_group_owner, interest_group_name},
      /*on_navigate_callback=*/base::RepeatingClosure(),
      ad_component_descriptors);

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(top_level_url, observer.mapped_url());
  EXPECT_EQ(interest_group_owner,
            observer.ad_auction_data()->interest_group_owner);
  EXPECT_EQ(interest_group_name,
            observer.ad_auction_data()->interest_group_name);
  EXPECT_TRUE(observer.nested_urn_config_pairs());
  EXPECT_FALSE(observer.on_navigate_callback());

  // Call with `add_to_new_map` set to false and true, to simulate ShadowDOM
  // and MPArch behavior, respectively.
  ValidatePendingAdComponentsMap(
      &fenced_frame_url_mapping,
      /*add_to_new_map=*/true, *observer.nested_urn_config_pairs(),
      /*expected_mapped_ad_descriptors=*/ad_component_descriptors);
  ValidatePendingAdComponentsMap(
      &fenced_frame_url_mapping,
      /*add_to_new_map=*/false, *observer.nested_urn_config_pairs(),
      /*expected_mapped_ad_descriptors=*/ad_component_descriptors);
}

// Test the case `ad_component_descriptors` has a single URL.
TEST_F(FencedFrameURLMappingTest, SubstituteFencedFrameURLs) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL top_level_url(
      "https://foo.test/page?%%TT%%${oo%%}p%%${p%%${%%l}%%%%%%%%evl%%");
  url::Origin interest_group_owner = url::Origin::Create(top_level_url);
  std::string interest_group_name = "bars";
  std::vector<blink::AdDescriptor> ad_component_descriptors{
      blink::AdDescriptor(GURL("https://bar.test/page?${REPLACED}"))};

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  fenced_frame_url_mapping.AssignFencedFrameURLAndInterestGroupInfo(
      urn_uuid, /*container_size=*/std::nullopt,
      blink::AdDescriptor(top_level_url),
      {interest_group_owner, interest_group_name},
      /*on_navigate_callback=*/base::RepeatingClosure(),
      ad_component_descriptors);

  fenced_frame_url_mapping.SubstituteMappedURL(
      urn_uuid,
      {{"%%notPresent%%",
        "not inserted"},               // replacements not present not inserted
       {"%%TT%%", "t"},                // %% replacement works
       {"${oo%%}", "o"},               // mixture of sequences works
       {"%%${p%%${%%l}%%%%%%", "_l"},  // mixture of sequences works
       {"${%%l}", "Don't replace"},    // earlier replacements take precedence
       {"%%evl%%", "evel_%%still_got_it%%"},  // output can contain
                                              // replacement sequences
       {"%%still_got_it%%",
        "not replaced"},                // output of replacement is not replaced
       {"${REPLACED}", "component"}});  // replacements affect components

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(GURL("https://foo.test/page?top_level_%%still_got_it%%"),
            observer.mapped_url());
  EXPECT_EQ(interest_group_owner,
            observer.ad_auction_data()->interest_group_owner);
  EXPECT_EQ(interest_group_name,
            observer.ad_auction_data()->interest_group_name);
  EXPECT_TRUE(observer.nested_urn_config_pairs());
  EXPECT_FALSE(observer.on_navigate_callback());

  // Call with `add_to_new_map` set to false and true, to simulate
  // ShadowDOM and MPArch behavior, respectively.
  std::vector<blink::AdDescriptor> expected_ad_component_descriptors{
      blink::AdDescriptor(GURL("https://bar.test/page?component"))};
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/true,
                                 *observer.nested_urn_config_pairs(),
                                 expected_ad_component_descriptors);
  ValidatePendingAdComponentsMap(&fenced_frame_url_mapping,
                                 /*add_to_new_map=*/false,
                                 *observer.nested_urn_config_pairs(),
                                 expected_ad_component_descriptors);
}

// Test the correctness of the URN format. The URN is expected to be in the
// format "urn:uuid:xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" as per RFC-4122.
TEST_F(FencedFrameURLMappingTest, HasCorrectFormat) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL test_url("https://foo.test");
  std::optional<GURL> urn_uuid =
      fenced_frame_url_mapping.AddFencedFrameURLForTesting(test_url);
  EXPECT_TRUE(urn_uuid.has_value());
  std::string spec = urn_uuid->spec();

  ASSERT_TRUE(base::StartsWith(
      spec, "urn:uuid:", base::CompareCase::INSENSITIVE_ASCII));

  EXPECT_EQ(spec.at(17), '-');
  EXPECT_EQ(spec.at(22), '-');
  EXPECT_EQ(spec.at(27), '-');
  EXPECT_EQ(spec.at(32), '-');

  EXPECT_TRUE(blink::IsValidUrnUuidURL(urn_uuid.value()));
}

// Test that reporting metadata gets saved successfully.
TEST_F(FencedFrameURLMappingTest, ReportingMetadataSuccess) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter = CreateReporter();
  GURL test_url("https://foo.test");
  std::optional<GURL> urn_uuid =
      fenced_frame_url_mapping.AddFencedFrameURLForTesting(
          test_url, fenced_frame_reporter);
  EXPECT_TRUE(urn_uuid.has_value());
  EXPECT_TRUE(urn_uuid->is_valid());
  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid.value(),
                                                      &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(fenced_frame_reporter.get(), observer.fenced_frame_reporter());
}

// Test that reporting metadata gets saved successfully.
TEST_F(FencedFrameURLMappingTest, ReporterSuccessWithInterestGroupInfo) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter = CreateReporter();
  GURL top_level_url("https://bar.test");
  url::Origin interest_group_owner = url::Origin::Create(top_level_url);
  std::string interest_group_name = "bars";
  std::vector<blink::AdDescriptor> ad_component_descriptors;

  auto urn_uuid = GenerateAndVerifyPendingMappedURN(&fenced_frame_url_mapping);

  fenced_frame_url_mapping.AssignFencedFrameURLAndInterestGroupInfo(
      urn_uuid, /*container_size=*/std::nullopt,
      blink::AdDescriptor(top_level_url),
      {interest_group_owner, interest_group_name},
      /*on_navigate_callback=*/base::RepeatingClosure(),
      ad_component_descriptors, fenced_frame_reporter);

  TestFencedFrameURLMappingResultObserver observer;
  fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid, &observer);
  EXPECT_TRUE(observer.mapping_complete_observed());
  EXPECT_EQ(fenced_frame_reporter.get(), observer.fenced_frame_reporter());
}

// Test that number of urn mappings limit is enforced for pending mapped urn
// generation.
TEST_F(FencedFrameURLMappingTest, ExceedNumOfUrnMappingsLimitFailsAddURL) {
  FencedFrameURLMapping fenced_frame_url_mapping;

  // Able to generate pending mapped URN when map is not full.
  EXPECT_TRUE(fenced_frame_url_mapping.GeneratePendingMappedURN().has_value());

  // Able to add urn mapping when map is not full.
  const GURL test_url("https://test.test");
  std::optional<GURL> urn_uuid =
      fenced_frame_url_mapping.AddFencedFrameURLForTesting(test_url);
  EXPECT_TRUE(urn_uuid.has_value());

  // Fill the map until its size reaches the limit.
  FencedFrameURLMappingTestPeer fenced_frame_url_mapping_test_peer(
      &fenced_frame_url_mapping);
  GURL url("https://a.test");
  fenced_frame_url_mapping_test_peer.FillMap(url);

  // Cannot generate pending mapped URN when map is full.
  EXPECT_FALSE(fenced_frame_url_mapping.GeneratePendingMappedURN().has_value());

  // Subsequent additions of urn mapping should fail when map is full.
  const GURL extra_url("https://extra.test");
  std::optional<GURL> extra_urn_uuid =
      fenced_frame_url_mapping.AddFencedFrameURLForTesting(extra_url);
  EXPECT_FALSE(extra_urn_uuid.has_value());
}

}  // namespace content
