// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_auth_challenge_util.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "net/http/http_auth_scheme.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_version.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/scheme_host_port.h"

namespace {

constexpr char kProxyHost[] = "proxy.example.com";
constexpr NSInteger kProxyPort = 8443;

// Builds a proxy protection space with sensible defaults, so that individual
// tests only specify the property they exercise. No realm is passed: Foundation
// discards the realm of a proxy protection space, so it can only be recovered
// from the `Proxy-Authenticate` header.
NSURLProtectionSpace* MakeProxyProtectionSpace(
    NSString* host = @"proxy.example.com",
    NSInteger port = kProxyPort,
    NSString* proxy_type = NSURLProtectionSpaceHTTPSProxy,
    NSString* auth_method = NSURLAuthenticationMethodHTTPBasic) {
  return [[NSURLProtectionSpace alloc] initWithProxyHost:host
                                                    port:port
                                                    type:proxy_type
                                                   realm:nil
                                    authenticationMethod:auth_method];
}

// Builds a non-proxy (server) protection space.
NSURLProtectionSpace* MakeServerProtectionSpace() {
  return [[NSURLProtectionSpace alloc]
              initWithHost:@"server.example.com"
                      port:443
                  protocol:NSURLProtectionSpaceHTTPS
                     realm:@"Server Realm"
      authenticationMethod:NSURLAuthenticationMethodHTTPBasic];
}

// Builds a 407 response carrying `headers`.
NSHTTPURLResponse* Make407Response(
    NSDictionary<NSString*, NSString*>* headers) {
  return [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://destination.example.com/page"]
        statusCode:407
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
}

using ProxyAuthChallengeUtilTest = PlatformTest;

#pragma mark - CreateProxyAuthHeadersFromNSURLResponse

// Test that a 407 response is converted into headers preserving the status
// code and the challenge header.
TEST_F(ProxyAuthChallengeUtilTest, CreateHeadersFromProxyAuthResponse) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      CreateProxyAuthHeadersFromNSURLResponse(
          Make407Response(@{@"Proxy-Authenticate" : @"Basic realm=\"503\""}));

  ASSERT_TRUE(headers);
  EXPECT_EQ(407, headers->response_code());
  EXPECT_TRUE(
      headers->HasHeaderValue("Proxy-Authenticate", "Basic realm=\"503\""));
}

// Test that a nil response yields no headers.
TEST_F(ProxyAuthChallengeUtilTest, CreateHeadersFromNilResponse) {
  EXPECT_FALSE(CreateProxyAuthHeadersFromNSURLResponse(nil));
}

// Test that a response which is not an HTTP response yields no headers.
TEST_F(ProxyAuthChallengeUtilTest, CreateHeadersFromNonHTTPResponse) {
  NSURLResponse* response = [[NSURLResponse
      alloc] initWithURL:[NSURL
                             URLWithString:@"https://destination.example.com"]
                   MIMEType:@"text/html"
      expectedContentLength:0
           textEncodingName:nil];

  EXPECT_FALSE(CreateProxyAuthHeadersFromNSURLResponse(response));
}

#pragma mark - CreateAuthChallengeInfoFromProtectionSpace

// Test that an HTTPS proxy challenge produces a valid `https` challenger.
TEST_F(ProxyAuthChallengeUtilTest, ConvertHTTPSProxyChallenge) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(MakeProxyProtectionSpace(),
                                                 /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->is_proxy);
  EXPECT_TRUE(auth_info->challenger.IsValid());
  EXPECT_EQ("https", auth_info->challenger.scheme());
  EXPECT_EQ(kProxyHost, auth_info->challenger.host());
  EXPECT_EQ(kProxyPort, auth_info->challenger.port());
  EXPECT_EQ(GURL("https://proxy.example.com:8443"),
            auth_info->challenger.GetURL());
  EXPECT_EQ("/", auth_info->path);
}

// Test that an HTTP proxy challenge produces a valid `http` challenger.
TEST_F(ProxyAuthChallengeUtilTest, ConvertHTTPProxyChallenge) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(
          MakeProxyProtectionSpace(/*host=*/@"proxy.example.com",
                                   /*port=*/8080,
                                   NSURLProtectionSpaceHTTPProxy),
          /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->challenger.IsValid());
  EXPECT_EQ("http", auth_info->challenger.scheme());
  EXPECT_EQ(8080, auth_info->challenger.port());
}

// Test that an uppercase host is canonicalized. PAC scripts and MDM profiles
// commonly advertise proxies this way, and Foundation does not normalize the
// host, while `url::SchemeHostPort` rejects a non-canonical one.
TEST_F(ProxyAuthChallengeUtilTest, ConvertUppercaseHost) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(
          MakeProxyProtectionSpace(/*host=*/@"PROXY.Example.COM"),
          /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->challenger.IsValid());
  EXPECT_EQ(kProxyHost, auth_info->challenger.host());
}

// Test that a bracketed IPv6 literal survives canonicalization, and that an
// unbracketed one is rejected: it cannot be told apart from a `host:port`
// pair, so no canonical form can be recovered from it.
TEST_F(ProxyAuthChallengeUtilTest, ConvertIPv6Host) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(
          MakeProxyProtectionSpace(/*host=*/@"[2001:db8::1]"),
          /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->challenger.IsValid());
  EXPECT_EQ("[2001:db8::1]", auth_info->challenger.host());

  EXPECT_FALSE(CreateAuthChallengeInfoFromProtectionSpace(
                   MakeProxyProtectionSpace(/*host=*/@"2001:db8::1"),
                   /*response_headers=*/nullptr)
                   .has_value());
}

// Test that an HTTPS proxy reporting no port falls back to port 443. Without
// this substitution the challenger would carry port 0, which is accepted by
// `url::SchemeHostPort` but matches no Provisioning Domain routing rule.
TEST_F(ProxyAuthChallengeUtilTest, ConvertHTTPSProxyChallengeWithoutPort) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(
          MakeProxyProtectionSpace(/*host=*/@"proxy.example.com", /*port=*/0),
          /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->challenger.IsValid());
  EXPECT_EQ(443, auth_info->challenger.port());
}

// Test that an HTTP proxy reporting no port falls back to port 80.
TEST_F(ProxyAuthChallengeUtilTest, ConvertHTTPProxyChallengeWithoutPort) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(
          MakeProxyProtectionSpace(/*host=*/@"proxy.example.com", /*port=*/0,
                                   NSURLProtectionSpaceHTTPProxy),
          /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->challenger.IsValid());
  EXPECT_EQ(80, auth_info->challenger.port());
}

// Test that each supported authentication method maps to its `net::` scheme.
TEST_F(ProxyAuthChallengeUtilTest, ConvertAuthenticationMethods) {
  const struct {
    NSString* method;
    std::string expected_scheme;
  } test_cases[] = {
      {NSURLAuthenticationMethodHTTPBasic, net::kBasicAuthScheme},
      {NSURLAuthenticationMethodDefault, net::kBasicAuthScheme},
      {NSURLAuthenticationMethodHTTPDigest, net::kDigestAuthScheme},
      {NSURLAuthenticationMethodNTLM, net::kNtlmAuthScheme},
      {NSURLAuthenticationMethodNegotiate, net::kNegotiateAuthScheme},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::SysNSStringToUTF8(test_case.method));
    std::optional<net::AuthChallengeInfo> auth_info =
        CreateAuthChallengeInfoFromProtectionSpace(
            MakeProxyProtectionSpace(
                /*host=*/@"proxy.example.com", kProxyPort,
                NSURLProtectionSpaceHTTPSProxy, test_case.method),
            /*response_headers=*/nullptr);

    ASSERT_TRUE(auth_info.has_value());
    EXPECT_EQ(test_case.expected_scheme, auth_info->scheme);
  }
}

// Test that an authentication method which is not an HTTP scheme still
// converts, leaving the scheme empty.
TEST_F(ProxyAuthChallengeUtilTest, ConvertUnknownAuthenticationMethod) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(
          MakeProxyProtectionSpace(
              /*host=*/@"proxy.example.com", kProxyPort,
              NSURLProtectionSpaceHTTPSProxy,
              NSURLAuthenticationMethodClientCertificate),
          /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->scheme.empty());
}

// Test that the realm advertised by the challenge is carried over verbatim,
// since `EnterpriseProxyService` matches it against the supported error codes.
// The realm cannot come from the protection space: Foundation always reports a
// nil realm for proxy protection spaces.
TEST_F(ProxyAuthChallengeUtilTest, ConvertDisguisedErrorRealm) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      CreateProxyAuthHeadersFromNSURLResponse(
          Make407Response(@{@"Proxy-Authenticate" : @"Basic realm=\"503\""}));

  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(MakeProxyProtectionSpace(),
                                                 headers);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_EQ("503", auth_info->realm);
}

// Test the `Proxy-Authenticate` shapes the Security Gateway actually emits.
// A genuine challenge carries the OAuth realm, while a disguised error carries
// the real status code as the realm plus error-specific parameters.
//
// The realm is parsed the way desktop parses it, so several of these shapes
// yield a realm desktop would also get wrong. Each such row is marked below
// with what is lost. This is a deliberate trade: one parser shared with every
// other platform, whose quirks are fixed once, upstream.
TEST_F(ProxyAuthChallengeUtilTest, ConvertSecurityGatewayChallengeShapes) {
  const struct {
    NSString* header_value;
    std::string expected_realm;
  } test_cases[] = {
      // Genuine 407: credentials are only supplied for this exact realm.
      {@"Basic realm=\"oauthaccountmanager.googleapis.com\"",
       "oauthaccountmanager.googleapis.com"},
      // Disguised errors, whose realm is the real status code.
      {@"Basic realm=\"403\"", "403"},
      // Auth-params separated by a comma are split correctly.
      {@"Basic realm=\"503\",retry=true", "503"},
      // Auth-params separated by a space are NOT. Desktop splits parameters on
      // commas only, so the rest of the challenge is absorbed into the realm
      // and the disguised error is not recognised. `HttpUtil` recovers from the
      // resulting mismatched quotes by dropping the leading one; when the value
      // happens to end in a quote it is instead unquoted whole, which is why
      // these two shapes lose their leading quote but keep their inner ones.
      {@"Basic realm=\"503\" retry=true,delay=5", "503\" retry=true"},
      {@"Basic realm=\"403\" message=\"Access to XYZ was denied\"",
       "403\" message=\"Access to XYZ was denied"},
      {@"Basic realm=\"403\" url=\"http://error.mycompany.com/ctx=XYZ\"",
       "403\" url=\"http://error.mycompany.com/ctx=XYZ"},
      {@"Basic realm=\"403\" message=\"see realm=503 docs\"",
       "403\" message=\"see realm=503 docs"},
      // RFC 9110 allows whitespace on either side of the `=`, and leaves the
      // quotes optional for a token value. Both survive, because
      // `ParseNameValuePair` trims whitespace around the name and the value.
      {@"Basic realm = \"503\"", "503"},
      {@"Basic realm=503", "503"},
      // Auth-param names are case-insensitive.
      {@"Basic REALM=\"503\"", "503"},
      // Foundation merges repeated `Proxy-Authenticate` lines into a single
      // comma-joined value. `//net` never does this, precisely because the
      // result is ambiguous, so desktop has no behaviour to copy here: the
      // merged value parses as the single challenge its first auth-scheme
      // introduces. That scheme is not the one WebKit selected, so no realm is
      // found and the disguised error is missed.
      {@"Negotiate, Basic realm=\"503\"", ""},
      {@"Negotiate YII0abc==, Basic realm=\"503\"", ""},
      // An auth-param whose name merely ends in `realm` is a different
      // parameter.
      {@"Basic x-realm=\"503\"", ""},
      // A space-separated parameter before the realm absorbs it, for the same
      // reason as the rows above.
      {@"Basic message=\"x realm=503\" realm=\"403\"", ""},
      // A quoted-pair escapes a quote inside the value.
      {@"Basic realm=\"a\\\"b\"", "a\"b"},
      // An unterminated quote is recovered from by dropping the leading quote.
      {@"Basic realm=\"403", "403"},
      // Only the challenge whose auth-scheme WebKit selected is read. The
      // fixture's protection space is Basic, so a Digest realm is ignored.
      {@"Digest realm=\"503\", nonce=\"abc\"", ""},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::SysNSStringToUTF8(test_case.header_value));
    scoped_refptr<net::HttpResponseHeaders> headers =
        CreateProxyAuthHeadersFromNSURLResponse(
            Make407Response(@{@"Proxy-Authenticate" : test_case.header_value}));

    std::optional<net::AuthChallengeInfo> auth_info =
        CreateAuthChallengeInfoFromProtectionSpace(MakeProxyProtectionSpace(),
                                                   headers);

    ASSERT_TRUE(auth_info.has_value());
    EXPECT_EQ(test_case.expected_realm, auth_info->realm);
  }
}

// Test that a challenge advertising no realm converts into an empty realm.
TEST_F(ProxyAuthChallengeUtilTest, ConvertChallengeWithoutRealm) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      CreateProxyAuthHeadersFromNSURLResponse(
          Make407Response(@{@"Proxy-Authenticate" : @"Basic"}));

  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(MakeProxyProtectionSpace(),
                                                 headers);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->realm.empty());
}

// Test that absent response headers leave the realm empty, rather than
// inheriting the realm passed to the protection space, which Foundation drops.
TEST_F(ProxyAuthChallengeUtilTest, ConvertRealmWithoutResponseHeaders) {
  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(MakeProxyProtectionSpace(),
                                                 /*response_headers=*/nullptr);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_TRUE(auth_info->realm.empty());
}

// Test that a realm advertised by a later `Proxy-Authenticate` line is found.
// The headers are built directly because `NSHTTPURLResponse` cannot express
// repeated header fields.
TEST_F(ProxyAuthChallengeUtilTest, ConvertRealmFromRepeatedHeaders) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "407")
          .AddHeader("Proxy-Authenticate", "Negotiate")
          .AddHeader("Proxy-Authenticate", "Basic realm=\"403\"")
          .Build();

  std::optional<net::AuthChallengeInfo> auth_info =
      CreateAuthChallengeInfoFromProtectionSpace(MakeProxyProtectionSpace(),
                                                 headers);

  ASSERT_TRUE(auth_info.has_value());
  EXPECT_EQ("403", auth_info->realm);
}

#pragma mark - CreateAuthChallengeInfoFromProtectionSpace rejections

// Test that a nil protection space is rejected.
TEST_F(ProxyAuthChallengeUtilTest, RejectNilProtectionSpace) {
  EXPECT_FALSE(CreateAuthChallengeInfoFromProtectionSpace(
                   nil, /*response_headers=*/nullptr)
                   .has_value());
}

// Test that a server (non-proxy) challenge is rejected.
TEST_F(ProxyAuthChallengeUtilTest, RejectServerProtectionSpace) {
  EXPECT_FALSE(CreateAuthChallengeInfoFromProtectionSpace(
                   MakeServerProtectionSpace(), /*response_headers=*/nullptr)
                   .has_value());
}

// Test that proxy types without a standard URL scheme are rejected.
TEST_F(ProxyAuthChallengeUtilTest, RejectUnsupportedProxyTypes) {
  NSArray<NSString*>* unsupported_types = @[
    NSURLProtectionSpaceSOCKSProxy,
    NSURLProtectionSpaceFTPProxy,
  ];

  for (NSString* proxy_type in unsupported_types) {
    SCOPED_TRACE(base::SysNSStringToUTF8(proxy_type));
    EXPECT_FALSE(CreateAuthChallengeInfoFromProtectionSpace(
                     MakeProxyProtectionSpace(/*host=*/@"proxy.example.com",
                                              kProxyPort, proxy_type),
                     /*response_headers=*/nullptr)
                     .has_value());
  }
}

// Test that an empty host is rejected, since it would silently produce an
// invalid challenger.
TEST_F(ProxyAuthChallengeUtilTest, RejectEmptyHost) {
  EXPECT_FALSE(CreateAuthChallengeInfoFromProtectionSpace(
                   MakeProxyProtectionSpace(/*host=*/@""),
                   /*response_headers=*/nullptr)
                   .has_value());
}

// Test that a negative port is rejected. Unlike an absent port, a negative
// port is malformed and has no defensible default.
TEST_F(ProxyAuthChallengeUtilTest, RejectNegativePort) {
  EXPECT_FALSE(CreateAuthChallengeInfoFromProtectionSpace(
                   MakeProxyProtectionSpace(/*host=*/@"proxy.example.com",
                                            /*port=*/-1),
                   /*response_headers=*/nullptr)
                   .has_value());
}

// Test that a port above the valid range is rejected without crashing on the
// narrowing conversion.
TEST_F(ProxyAuthChallengeUtilTest, RejectOutOfRangePort) {
  EXPECT_FALSE(CreateAuthChallengeInfoFromProtectionSpace(
                   MakeProxyProtectionSpace(/*host=*/@"proxy.example.com",
                                            /*port=*/70000),
                   /*response_headers=*/nullptr)
                   .has_value());
}

}  // namespace
