// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"

#include <string>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_encoding_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class XMLHttpRequestTest : public PageTestBase {
 protected:
};

class FakeTextResourceDecoder final : public TextResourceDecoder {
 public:
  explicit FakeTextResourceDecoder(String decoded)
      : TextResourceDecoder(TextResourceDecoderOptions::CreateUTF8Decode()),
        decoded_(decoded) {}

  String Decode(base::span<const char>, String*) override { return decoded_; }
  String Flush() override { return String(); }
  WebEncodingData GetEncodingData() const override { return WebEncodingData(); }

 private:
  String decoded_;
};

// An XHR with an origin with `CanLoadLocalResources` set cannot set forbidden
// request headers. It was historically allowed, and this is a regression test.
// See https://crbug.com/567527 for details.
TEST_F(XMLHttpRequestTest, ForbiddenRequestHeaderWithLocalOrigin) {
  GetFrame().DomWindow()->GetMutableSecurityOrigin()->GrantLoadLocalResources();

  auto* xhr = XMLHttpRequest::Create(ToScriptStateForMainWorld(&GetFrame()));

  xhr->open(http_names::kGET, "https://example.com/", ASSERT_NO_EXCEPTION);
  xhr->setRequestHeader(AtomicString("host"), AtomicString("example.com"),
                        ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(xhr->HasRequestHeaderForTesting(AtomicString("host")));
}

TEST_F(XMLHttpRequestTest, ResponseTextUsesDecodedData) {
  static constexpr size_t kDecodedSize = 1024 * 1024;
  std::string decoded_data(kDecodedSize, 'x');

  auto* xhr = XMLHttpRequest::Create(ToScriptStateForMainWorld(&GetFrame()));
  xhr->decoder_ =
      std::make_unique<FakeTextResourceDecoder>(String::FromUTF8(decoded_data));

  const char kData[] = "x";
  xhr->DidReceiveData(base::span_from_cstring(kData));

  DummyExceptionStateForTesting exception_state;
  String response_text = xhr->responseText(exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(response_text.length(), kDecodedSize);
}

}  // namespace blink
