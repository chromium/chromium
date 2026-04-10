// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::webid {
namespace {
IdentityRequestAccountPtr CreateEmptyAccount() {
  std::vector<std::string> empty;
  return base::MakeRefCounted<IdentityRequestAccount>(
      /*id=*/"",
      /*display_identifier=*/"", /*display_name=*/"", /*email=*/"",
      /*name=*/"", /*given_name=*/"", /*picture=*/GURL(), /*phone=*/"",
      /*username=*/"", /*potentially_approved_site_hashes=*/empty,
      /*login_hints=*/empty, /*domain_hints=*/empty,
      /*labels=*/empty);
}
}  // namespace

TEST(FedCmMetricsTest, HasNonce) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  {
    Metrics metrics(ukm::AssignNewSourceId());
    GURL provider("https://idp.example");
    metrics.RecordHasNonce({provider});
  }

  histogram_tester.ExpectUniqueSample("Blink.FedCm.HasNonce", 1, 1);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Blink_FedCm::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(entries[0],
                                 ukm::builders::Blink_FedCm::kHasNonceName, 1);

  auto idp_entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Blink_FedCmIdp::kEntryName);
  ASSERT_EQ(1u, idp_entries.size());
  ukm_recorder.ExpectEntryMetric(
      idp_entries[0], ukm::builders::Blink_FedCmIdp::kHasNonceName, 1);
}

TEST(FedCmMetricsTest, HasNonceOutsideParamsOnly) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  {
    Metrics metrics(ukm::AssignNewSourceId());
    GURL provider("https://idp.example");
    metrics.RecordHasNonce({provider});
    metrics.RecordHasNonceOutsideParamsOnly({provider});
  }

  histogram_tester.ExpectUniqueSample("Blink.FedCm.HasNonce", 1, 1);
  histogram_tester.ExpectUniqueSample("Blink.FedCm.HasNonceOutsideParamsOnly",
                                      1, 1);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Blink_FedCm::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(entries[0],
                                 ukm::builders::Blink_FedCm::kHasNonceName, 1);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Blink_FedCm::kHasNonceOutsideParamsOnlyName,
      1);

  auto idp_entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Blink_FedCmIdp::kEntryName);
  ASSERT_EQ(1u, idp_entries.size());
  ukm_recorder.ExpectEntryMetric(
      idp_entries[0], ukm::builders::Blink_FedCmIdp::kHasNonceName, 1);
  ukm_recorder.ExpectEntryMetric(
      idp_entries[0],
      ukm::builders::Blink_FedCmIdp::kHasNonceOutsideParamsOnlyName, 1);
}

TEST(FedCmMetricsTest, WellKnownInvalidDueToClientMetadata) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  {
    Metrics metrics(ukm::AssignNewSourceId());
    GURL provider("https://idp.example");
    metrics.RecordWellKnownInvalidDueToClientMetadata(provider);
  }

  histogram_tester.ExpectUniqueSample(
      "Blink.FedCm.WellKnownInvalidDueToClientMetadata", 1, 1);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Blink_FedCm::kEntryName);
  ASSERT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::Blink_FedCm::kWellKnownInvalidDueToClientMetadataName, 1);

  auto idp_entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Blink_FedCmIdp::kEntryName);
  ASSERT_EQ(1u, idp_entries.size());
  ukm_recorder.ExpectEntryMetric(
      idp_entries[0],
      ukm::builders::Blink_FedCmIdp::kWellKnownInvalidDueToClientMetadataName,
      1);
}

TEST(FedCmMetricsTest, AccountFieldsTypeNameAndEmail) {
  base::HistogramTester histogram_tester_;

  auto account = CreateEmptyAccount();
  account->name = "Name";
  account->email = "name@email.example";

  RecordAccountFieldsType({account});

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.AccountFieldsType",
      static_cast<int>(webid::AccountFieldsType::kNameAndEmailAndNoOther), 1);
}

TEST(FedCmMetricsTest, AccountFieldsOnlyName) {
  base::HistogramTester histogram_tester_;

  auto account = CreateEmptyAccount();
  account->name = "Name";

  RecordAccountFieldsType({account});

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.AccountFieldsType",
      static_cast<int>(webid::AccountFieldsType::kOneOfNameAndEmailAndNoOther),
      1);
}

TEST(FedCmMetricsTest, AccountFieldsNameEmailAndPhone) {
  base::HistogramTester histogram_tester_;

  auto account = CreateEmptyAccount();
  account->name = "Name";
  account->email = "name@email.example";
  account->phone = "(01234) 567890";

  RecordAccountFieldsType({account});

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.AccountFieldsType",
      static_cast<int>(
          webid::AccountFieldsType::kNameOrEmailAndOtherIdentifier),
      1);
}

TEST(FedCmMetricsTest, AccountFieldsOnlyPhone) {
  base::HistogramTester histogram_tester_;

  auto account = CreateEmptyAccount();
  account->username = "Username";

  RecordAccountFieldsType({account});

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.AccountFieldsType",
      static_cast<int>(
          webid::AccountFieldsType::kOtherIdentifierButNoNameOrEmail),
      1);
}

}  // namespace content::webid
