// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/update_client/model/ios_chrome_update_query_params_delegate.h"

#include <string>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "components/update_client/update_query_params.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/common/channel_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

void TestParams(update_client::UpdateQueryParams::ProdId prod_id) {
  std::string params = update_client::UpdateQueryParams::Get(prod_id);

  EXPECT_TRUE(base::Contains(
      params,
      base::StrCat({"os=", update_client::UpdateQueryParams::GetOS()})));
  EXPECT_TRUE(base::Contains(
      params,
      base::StrCat({"arch=", update_client::UpdateQueryParams::GetArch()})));
  EXPECT_TRUE(base::Contains(
      params,
      base::StrCat({"prod=", update_client::UpdateQueryParams::GetProdIdString(
                                 prod_id)})));
  EXPECT_TRUE(base::Contains(
      params, base::StrCat({"prodchannel=", GetChannelString()})));
  EXPECT_TRUE(base::Contains(
      params,
      base::StrCat({"prodversion=", version_info::GetVersionNumber()})));
  EXPECT_TRUE(base::Contains(
      params,
      base::StrCat({"lang=", IOSChromeUpdateQueryParamsDelegate::GetLang()})));
}

using IOSChromeUpdateQueryParamsDelegateTest = PlatformTest;

TEST_F(IOSChromeUpdateQueryParamsDelegateTest, GetParams) {
  absl::Cleanup reset_delegate = [] {
    update_client::UpdateQueryParams::SetDelegate(nullptr);
  };
  update_client::UpdateQueryParams::SetDelegate(
      IOSChromeUpdateQueryParamsDelegate::GetInstance());

  TestParams(update_client::UpdateQueryParams::CHROME);
}
