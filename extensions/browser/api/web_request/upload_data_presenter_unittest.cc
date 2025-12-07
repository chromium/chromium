// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/upload_data_presenter.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/values.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace keys = extension_web_request_api_constants;

namespace extensions {

// This only tests the handling of dots in keys. Other functionality is covered
// by ExtensionWebRequestTest.AccessRequestBodyData and
// WebRequestFormDataParserTest.
TEST(WebRequestUploadDataPresenterTest, ParsedData) {
  // Input.
  const char block[] = "key.with.dots=value";
  net::UploadBytesElementReader element(base::byte_span_from_cstring(block));

  // Expected output.
  base::Value::List values;
  values.Append("value");
  base::Value::Dict expected_form;
  expected_form.Set("key.with.dots", std::move(values));

  // Real output.
  std::unique_ptr<ParsedDataPresenter> parsed_data_presenter(
      ParsedDataPresenter::CreateForTests());
  ASSERT_TRUE(parsed_data_presenter.get() != nullptr);
  parsed_data_presenter->FeedBytes(base::as_string_view(element.bytes()));
  EXPECT_TRUE(parsed_data_presenter->Succeeded());
  std::optional<base::Value> result = parsed_data_presenter->TakeResult();
  EXPECT_EQ(result, expected_form);
}

TEST(WebRequestUploadDataPresenterTest, RawData) {
  // Input.
  auto block1 = base::byte_span_from_cstring("test");
  const char kFilename[] = "path/test_filename.ext";
  auto block2 = base::byte_span_from_cstring("another test");

  // Expected output.
  base::Value expected_a(block1);
  base::Value expected_b(kFilename);
  base::Value expected_c(block2);

  base::Value::List expected_list;
  subtle::AppendKeyValuePair(keys::kRequestBodyRawBytesKey,
                             std::move(expected_a), expected_list);
  subtle::AppendKeyValuePair(keys::kRequestBodyRawFileKey,
                             std::move(expected_b), expected_list);
  subtle::AppendKeyValuePair(keys::kRequestBodyRawBytesKey,
                             std::move(expected_c), expected_list);

  // Real output.
  RawDataPresenter raw_presenter;
  raw_presenter.FeedNextBytes(block1);
  raw_presenter.FeedNextFile(kFilename);
  raw_presenter.FeedNextBytes(block2);
  EXPECT_TRUE(raw_presenter.Succeeded());
  std::optional<base::Value> result = raw_presenter.TakeResult();
  EXPECT_EQ(expected_list, result);
}

}  // namespace extensions
