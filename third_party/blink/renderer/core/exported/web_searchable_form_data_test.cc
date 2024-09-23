/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_searchable_form_data.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

void RegisterMockedURLLoadFromBaseURL(const std::string& base_url,
                                      const std::string& file_name) {
  // TODO(crbug.com/751425): We should use the mock functionality
  // via |WebSearchableFormDataTest::web_view_helper_|.
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8(file_name));
}

class WebSearchableFormDataTest : public testing::Test {
 protected:
  WebSearchableFormDataTest() = default;

  ~WebSearchableFormDataTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

}  // namespace
TEST_F(WebSearchableFormDataTest, HttpSearchString) {
  std::string base_url("http://www.test.com/");
  RegisterMockedURLLoadFromBaseURL(base_url, "search_form_http.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url + "search_form_http.html");

  WebVector<WebFormElement> forms =
      web_view->MainFrameImpl()->GetDocument().Forms();

  EXPECT_EQ(forms.size(), 1U);

  WebSearchableFormData searchable_form_data(forms[0]);
  EXPECT_EQ("http://www.mock.url/search?hl=en&q={searchTerms}&btnM=Mock+Search",
            searchable_form_data.Url().GetString());
}

TEST_F(WebSearchableFormDataTest, HttpsSearchString) {
  std::string base_url("https://www.test.com/");
  RegisterMockedURLLoadFromBaseURL(base_url, "search_form_https.html");
  WebViewImpl* web_view =
      web_view_helper_.InitializeAndLoad(base_url + "search_form_https.html");

  WebVector<WebFormElement> forms =
      web_view->MainFrameImpl()->GetDocument().Forms();

  EXPECT_EQ(forms.size(), 1U);

  WebSearchableFormData searchable_form_data(forms[0]);
  EXPECT_EQ(
      "https://www.mock.url/search?hl=en&q={searchTerms}&btnM=Mock+Search",
      searchable_form_data.Url().GetString());
}

}  // namespace blink
