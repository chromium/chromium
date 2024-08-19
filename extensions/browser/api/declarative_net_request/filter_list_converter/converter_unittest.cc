// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/filter_list_converter/converter.h"

#include <optional>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::declarative_net_request::filter_list_converter {
namespace {

void TestConversion(std::vector<std::string> filter_list_rules,
                    std::string json_result,
                    WriteType write_type) {
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());

  base::FilePath input_path = temp_dir.GetPath().AppendASCII("filterlist.txt");
  std::string filterlist = base::JoinString(filter_list_rules, "\n");
  CHECK(base::WriteFile(input_path, filterlist));

  std::optional<base::Value> expected_json =
      base::JSONReader::Read(json_result);
  CHECK(expected_json.has_value());

  base::FilePath output_path = temp_dir.GetPath();
  if (write_type == WriteType::kJSONRuleset) {
    output_path = output_path.AppendASCII("rules.json");
  }

  ConvertRuleset({input_path}, output_path, write_type, true /* noisy */);

  base::FilePath output_json_path =
      temp_dir.GetPath().AppendASCII("rules.json");
  JSONFileValueDeserializer deserializer(output_json_path);
  std::unique_ptr<base::Value> actual_json = deserializer.Deserialize(
      nullptr /* error_code */, nullptr /* error_message */);
  ASSERT_TRUE(actual_json.get());

  EXPECT_EQ(*expected_json, *actual_json);

  if (write_type == WriteType::kExtension) {
    EXPECT_TRUE(
        base::PathExists(temp_dir.GetPath().AppendASCII("manifest.json")));
  }
}

class FilterListConverterTest : public ::testing::TestWithParam<WriteType> {};

TEST_P(FilterListConverterTest, Convert) {
  std::vector<std::string> filter_list_rules = {
      "||example.com^|$script,image,font",
      "@@allowed.com$domain=example.com|~sub.example.com",
      "|https://*.abc.com|$match-case,~image,third-party",
      "abc.com$~third-party",
      "@@||yahoo.com^$document,subdocument",
      "@@||yahoo.com^$document",

      // This rule is invalid and should be ignored.
      "@@||yahoo.com^$document,script",

      "@@||yahoo.com^$subdocument",
  };

  std::string expected_result = R"(
    [ {
       "action": {
          "type": "block"
       },
       "condition": {
          "isUrlFilterCaseSensitive": false,
          "resourceTypes": [ "script", "image", "font" ],
          "urlFilter": "||example.com^|"
       },
       "id": 1,
       "priority": 1
    }, {
       "action": {
          "type": "allow"
       },
       "condition": {
          "domains": [ "example.com" ],
          "excludedDomains": [ "sub.example.com" ],
          "isUrlFilterCaseSensitive": false,
          "urlFilter": "allowed.com"
       },
       "id": 2,
       "priority": 1
    }, {
       "action": {
          "type": "block"
       },
       "condition": {
          "resourceTypes": [ "other", "script", "stylesheet", "object",
              "xmlhttprequest", "sub_frame", "ping", "media", "font",
              "websocket", "webtransport", "webbundle"],
          "urlFilter": "|https://*.abc.com|",
          "domainType": "thirdParty"
       },
       "id": 3,
       "priority": 1
    }, {
       "action": {
          "type": "block"
       },
       "condition": {
          "isUrlFilterCaseSensitive": false,
          "urlFilter": "abc.com",
          "domainType": "firstParty"
       },
       "id": 4,
       "priority": 1
    }, {
       "action": {
          "type": "allowAllRequests"
       },
       "condition": {
          "isUrlFilterCaseSensitive": false,
          "resourceTypes": [ "sub_frame", "main_frame" ],
          "urlFilter": "||yahoo.com^"
       },
       "id": 5,
       "priority": 1
    }, {
       "action": {
          "type": "allowAllRequests"
       },
       "condition": {
          "isUrlFilterCaseSensitive": false,
          "resourceTypes": [ "main_frame" ],
          "urlFilter": "||yahoo.com^"
       },
       "id": 6,
       "priority": 1
    }, {
       "action": {
          "type": "allow"
       },
       "condition": {
          "isUrlFilterCaseSensitive": false,
          "resourceTypes": [ "sub_frame" ],
          "urlFilter": "||yahoo.com^"
       },
       "id": 7,
       "priority": 1
    } ]

)";

  TestConversion(filter_list_rules, expected_result, GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         FilterListConverterTest,
                         ::testing::Values(WriteType::kExtension,
                                           WriteType::kJSONRuleset));

}  // namespace
}  // namespace extensions::declarative_net_request::filter_list_converter
