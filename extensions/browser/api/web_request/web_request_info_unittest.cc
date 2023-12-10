// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_info.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/json/json_writer.h"

namespace extensions {
namespace {
constexpr char kFilePath[] = "some_path";
}

TEST(WebRequestInfoTest, CreateRequestBodyDataFromFile) {
  content::BrowserTaskEnvironment task_environment_;

  network::ResourceRequest request;
  request.method = "POST";
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);
  request.request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request.request_body->AppendFileRange(base::FilePath::FromASCII(kFilePath), 0,
                                        std::numeric_limits<uint64_t>::max(),
                                        base::Time());
  WebRequestInfo info(WebRequestInfoInitParams(0, 0, 0, nullptr, request, false,
                                               false, false, std::nullopt));
  ASSERT_TRUE(info.request_body_data);
  base::Value* value = info.request_body_data->Find(
      extension_web_request_api_constants::kRequestBodyRawKey);
  ASSERT_TRUE(value);

  base::Value expected_value(base::Value::Type::LIST);
  base::Value::Dict dict;
  dict.Set(extension_web_request_api_constants::kRequestBodyRawFileKey,
           kFilePath);
  expected_value.GetList().Append(std::move(dict));
  EXPECT_EQ(*value, expected_value);
}

}  // namespace extensions
