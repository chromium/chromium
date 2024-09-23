// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the ResponseAnalyzerTests (which test the response
// analyzer's behavior in several parameterized test scenarios) and at the end
// includes the CrossOriginReadBlockingTests, which are more typical unittests.

#include "services/network/orb/orb_mimetypes.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::orb {

TEST(CrossOriginReadBlockingTest, GetCanonicalMimeType) {
  std::vector<std::pair<const char*, MimeType>> tests = {
      // Basic tests for things in the original implementation:
      {"text/html", MimeType::kHtml},
      {"text/xml", MimeType::kXml},
      {"application/rss+xml", MimeType::kXml},
      {"application/xml", MimeType::kXml},
      {"application/json", MimeType::kJson},
      {"text/json", MimeType::kJson},
      {"text/plain", MimeType::kPlain},

      // Other mime types:
      {"application/foobar", MimeType::kOthers},

      // Regression tests for https://crbug.com/799155 (prefix/suffix matching):
      {"application/activity+json", MimeType::kJson},
      {"text/foobar+xml", MimeType::kXml},
      // No match without a '+' character:
      {"application/jsonfoobar", MimeType::kOthers},
      {"application/foobarjson", MimeType::kOthers},
      {"application/xmlfoobar", MimeType::kOthers},
      {"application/foobarxml", MimeType::kOthers},

      // Case-insensitive comparison:
      {"APPLICATION/JSON", MimeType::kJson},
      {"APPLICATION/ACTIVITY+JSON", MimeType::kJson},
      {"appLICAtion/zIP", MimeType::kNeverSniffed},

      // Images are allowed cross-site, and SVG is an image, so we should
      // classify SVG as "other" instead of "xml" (even though it technically is
      // an xml document).  Same argument for DASH video format.
      {"image/svg+xml", MimeType::kOthers},
      {"application/dash+xml", MimeType::kOthers},

      // Javascript should not be blocked.
      {"application/javascript", MimeType::kOthers},
      {"application/jsonp", MimeType::kOthers},

      // TODO(lukasza): Remove in the future, once this MIME type is not used in
      // practice.  See also https://crbug.com/826756#c3
      {"application/json+protobuf", MimeType::kJson},
      {"APPLICATION/JSON+PROTOBUF", MimeType::kJson},

      // According to specs, these types are not XML or JSON.  See also:
      // - https://mimesniff.spec.whatwg.org/#xml-mime-type
      // - https://mimesniff.spec.whatwg.org/#json-mime-type
      {"text/x-json", MimeType::kOthers},
      {"text/json+blah", MimeType::kOthers},
      {"application/json+blah", MimeType::kOthers},
      {"text/xml+blah", MimeType::kOthers},
      {"application/xml+blah", MimeType::kOthers},

      // Types protected without sniffing.
      {"application/gzip", MimeType::kNeverSniffed},
      {"application/pdf", MimeType::kNeverSniffed},
      {"application/x-protobuf", MimeType::kNeverSniffed},
      {"application/x-gzip", MimeType::kNeverSniffed},
      {"application/zip", MimeType::kNeverSniffed},
      {"multipart/byteranges", MimeType::kNeverSniffed},
      {"multipart/signed", MimeType::kNeverSniffed},
      {"text/csv", MimeType::kNeverSniffed},
      {"text/event-stream", MimeType::kNeverSniffed},
  };

  for (const auto& test : tests) {
    const char* input = test.first;  // e.g. "text/html"
    MimeType expected = test.second;
    MimeType actual = GetCanonicalMimeType(input);
    EXPECT_EQ(expected, actual)
        << "when testing with the following input: " << input;
  }
}

}  // namespace network::orb
