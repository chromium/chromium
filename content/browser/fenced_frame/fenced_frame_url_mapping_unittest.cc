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
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/fenced_frame_test_utils.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

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
  scoped_refptr<FencedFrameReporter> CreateSharedStorageReporter() {
    return FencedFrameReporter::CreateForSharedStorage(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            nullptr),
        /*browser_context=*/browser_context(),
        /*reporting_url_declarer_origin=*/std::nullopt,
        /*reporting_url_map=*/FencedFrameReporter::ReportingUrlMap());
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::HistogramTester histogram_tester_;
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
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      CreateSharedStorageReporter();
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
