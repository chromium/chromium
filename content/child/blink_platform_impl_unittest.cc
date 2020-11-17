// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/origin.h"

namespace content {

void CheckCastedOriginsAlreadyNormalized(
    const blink::WebSecurityOrigin& origin) {
  if (origin.IsOpaque())
    return;

  base::Optional<url::Origin> checked_origin =
      url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
          origin.Protocol().Utf8(), origin.Host().Utf8(),
          origin.EffectivePort());
  url::Origin non_checked_origin = url::Origin::CreateFromNormalizedTuple(
      origin.Protocol().Utf8(), origin.Host().Utf8(), origin.EffectivePort());
  EXPECT_EQ(checked_origin, non_checked_origin);
}

TEST(BlinkPlatformTest, CastWebSecurityOrigin) {
  struct TestCase {
    const char* url;
    const char* scheme;
    const char* host;
    uint16_t port;
  } cases[] = {
      {"http://example.com", "http", "example.com", 80},
      {"http://example.com:80", "http", "example.com", 80},
      {"http://example.com:81", "http", "example.com", 81},
      {"https://example.com", "https", "example.com", 443},
      {"https://example.com:443", "https", "example.com", 443},
      {"https://example.com:444", "https", "example.com", 444},

      // Copied from url/origin_unittest.cc

      // IP Addresses
      {"http://192.168.9.1/", "http", "192.168.9.1", 80},
      {"http://[2001:db8::1]/", "http", "[2001:db8::1]", 80},

      // Punycode
      {"http://☃.net/", "http", "xn--n3h.net", 80},
      {"blob:http://☃.net/", "http", "xn--n3h.net", 80},

      // Generic URLs
      {"http://example.com/", "http", "example.com", 80},
      {"http://example.com:123/", "http", "example.com", 123},
      {"https://example.com/", "https", "example.com", 443},
      {"https://example.com:123/", "https", "example.com", 123},
      {"http://user:pass@example.com/", "http", "example.com", 80},
      {"http://example.com:123/?query", "http", "example.com", 123},
      {"https://example.com/#1234", "https", "example.com", 443},
      {"https://u:p@example.com:123/?query#1234", "https", "example.com", 123},

      // Registered URLs
      {"ftp://example.com/", "ftp", "example.com", 21},
      {"ws://example.com/", "ws", "example.com", 80},
      {"wss://example.com/", "wss", "example.com", 443},

      // file: URLs
      {"file:///etc/passwd", "file", "", 0},
      {"file://example.com/etc/passwd", "file", "example.com", 0},

      // Filesystem:
      {"filesystem:http://example.com/type/", "http", "example.com", 80},
      {"filesystem:http://example.com:123/type/", "http", "example.com", 123},
      {"filesystem:https://example.com/type/", "https", "example.com", 443},
      {"filesystem:https://example.com:123/type/", "https", "example.com", 123},

      // Blob:
      {"blob:http://example.com/guid-goes-here", "http", "example.com", 80},
      {"blob:http://example.com:123/guid-goes-here", "http", "example.com",
       123},
      {"blob:https://example.com/guid-goes-here", "https", "example.com", 443},
      {"blob:http://u:p@example.com/guid-goes-here", "http", "example.com", 80},
      {"blob:https://example.co.uk/guid-goes-here", "https", "example.co.uk",
       443},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.url);
    blink::WebSecurityOrigin web_origin =
        blink::WebSecurityOrigin::CreateFromString(
            blink::WebString::FromUTF8(test.url));
    EXPECT_EQ(test.scheme, web_origin.Protocol().Utf8());
    EXPECT_EQ(test.host, web_origin.Host().Utf8());
    EXPECT_EQ(test.port, web_origin.EffectivePort());

    url::Origin url_origin = web_origin;
    EXPECT_EQ(test.scheme, url_origin.scheme());
    EXPECT_EQ(test.host, url_origin.host());
    EXPECT_EQ(test.port, url_origin.port());

    web_origin = url::Origin::Create(GURL(test.url));
    EXPECT_EQ(test.scheme, web_origin.Protocol().Utf8());
    EXPECT_EQ(test.host, web_origin.Host().Utf8());
    EXPECT_EQ(test.port, web_origin.EffectivePort());

    CheckCastedOriginsAlreadyNormalized(web_origin);
  }

  {
    SCOPED_TRACE(testing::Message() << "null");
    blink::WebSecurityOrigin web_origin =
        blink::WebSecurityOrigin::CreateUniqueOpaque();
    EXPECT_TRUE(web_origin.IsOpaque());

    url::Origin url_origin = web_origin;
    EXPECT_TRUE(url_origin.opaque());

    web_origin = url::Origin::Create(GURL(""));
    EXPECT_TRUE(web_origin.IsOpaque());
  }
}

// This test ensures that WebSecurityOrigins can safely use
// url::Origin::CreateFromNormalizedTuple when doing conversions.
TEST(BlinkPlatformTest, WebSecurityOriginNormalization) {
  struct TestCases {
    const char* url;
  } cases[] = {{""},
               {"javascript:alert(1)"},
               {"file://example.com:443/etc/passwd"},
               {"blob:https://example.com/uuid-goes-here"},
               {"filesystem:https://example.com/temporary/yay.png"},
               {"data"},
               {"blob:"},
               {"chrome://,/"},
               {"xkcd://927"},
               {"filesystem"},
               {"data://example.com:80"},
               {"http://☃.net:80"},
               {"http\nmore://example.com:80"},
               {"http\rmore://:example.com:80"},
               {"http\n://example.com:80"},
               {"http\r://example.com:80"},
               {"http://example.com\nnot-example.com:80"},
               {"http://example.com\rnot-example.com:80"},
               {"http://example.com\n:80"},
               {"http://example.com\r:80"},
               {"http://example.com:0"},
               {"http://EXAMPLE.com"},
               {"http://EXAMPLE.com/%3Afoo"},
               {"https://example.com:443"},
               {"file:///"},
               {"file:///root:80"}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << test.url);
    blink::WebSecurityOrigin web_origin =
        blink::WebSecurityOrigin::CreateFromString(
            blink::WebString::FromUTF8(test.url));
    CheckCastedOriginsAlreadyNormalized(web_origin);
  }
}

}  // namespace content
