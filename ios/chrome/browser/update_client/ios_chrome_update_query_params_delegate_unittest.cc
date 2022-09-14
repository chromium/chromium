// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/update_client/ios_chrome_update_query_params_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "components/update_client/update_query_params.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/common/channel_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::StringPrintf;

namespace {

bool Contains(const std::string& source, const std::string& target) {
  return source.find(target) != std::string::npos;
}

}  // namespace

void TestParams(update_client::UpdateQueryParams::ProdId prod_id) {
  std::string params = update_client::UpdateQueryParams::Get(prod_id);

  EXPECT_TRUE(Contains(
      params,
      StringPrintf("os=%s", update_client::UpdateQueryParams::GetOS())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("arch=%s", update_client::UpdateQueryParams::GetArch())));
  EXPECT_TRUE(Contains(
      params, StringPrintf(
                  "prod=%s",
                  update_client::UpdateQueryParams::GetProdIdString(prod_id))));
  EXPECT_TRUE(Contains(
      params, StringPrintf("prodchannel=%s", GetChannelString().c_str())));
  EXPECT_TRUE(
      Contains(params, StringPrintf("prodversion=%s",
                                    version_info::GetVersionNumber().c_str())));
  EXPECT_TRUE(Contains(
      params,
      StringPrintf("lang=%s",
                   IOSChromeUpdateQueryParamsDelegate::GetLang().c_str())));
}

using IOSChromeUpdateQueryParamsDelegateTest = PlatformTest;

TEST_F(IOSChromeUpdateQueryParamsDelegateTest, GetParams) {
  base::ScopedClosureRunner runner(
      base::BindOnce(update_client::UpdateQueryParams::SetDelegate, nullptr));
  update_client::UpdateQueryParams::SetDelegate(
      IOSChromeUpdateQueryParamsDelegate::GetInstance());

  TestParams(update_client::UpdateQueryParams::CHROME);
}
