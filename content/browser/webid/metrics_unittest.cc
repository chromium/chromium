// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/public/browser/webid/identity_request_account.h"
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
      /*username=*/"", /*login_hints=*/empty, /*domain_hints=*/empty,
      /*labels=*/empty);
}
}  // namespace

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
