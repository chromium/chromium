// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/net/ip_address_space_util.h"

#include <utility>

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {
namespace {

using net::IPAddress;
using net::IPEndPoint;
using network::mojom::ContentSecurityPolicy;
using network::mojom::IPAddressSpace;
using network::mojom::ParsedHeaders;
using network::mojom::URLResponseHead;

IPAddress PublicIPv4Address() {
  return IPAddress(64, 233, 160, 0);
}

IPAddress PrivateIPv4Address() {
  return IPAddress(192, 168, 1, 1);
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceFileURL) {
  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateClientAddressSpace(GURL("file:///foo"), nullptr));
}

TEST(IPAddressSpaceUtilTest,
     CalculateClientAddressSpaceFetchedViaServiceWorkerFromFile) {
  URLResponseHead response_head;
  response_head.url_list_via_service_worker.emplace_back("http://bar.test");
  response_head.url_list_via_service_worker.emplace_back("file:///foo");
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(
      IPAddressSpace::kLocal,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceUtilTest,
     CalculateClientAddressSpaceFetchedViaServiceWorkerFromHttp) {
  URLResponseHead response_head;
  response_head.url_list_via_service_worker.emplace_back("file:///foo");
  response_head.url_list_via_service_worker.emplace_back("http://bar.test");
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(
      IPAddressSpace::kUnknown,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceUtilTest,
     CalculateClientAddressSpaceFetchedViaServiceWorkerFromHttpInsteadOfFile) {
  URLResponseHead response_head;
  response_head.url_list_via_service_worker.emplace_back("http://bar.test");
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(IPAddressSpace::kUnknown,
            CalculateClientAddressSpace(GURL("file:///foo"), &response_head));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceNullResponseHead) {
  EXPECT_EQ(IPAddressSpace::kUnknown,
            CalculateClientAddressSpace(GURL("http://foo.test"), nullptr));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceEmptyResponseHead) {
  URLResponseHead response_head;
  response_head.parsed_headers = ParsedHeaders::New();
  EXPECT_EQ(
      IPAddressSpace::kUnknown,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceIPAddress) {
  URLResponseHead response_head;
  response_head.remote_endpoint = IPEndPoint(PrivateIPv4Address(), 1234);
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(
      IPAddressSpace::kPrivate,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceTreatAsPublicAddress) {
  URLResponseHead response_head;
  response_head.remote_endpoint = IPEndPoint(IPAddress::IPv4Localhost(), 1234);

  auto csp = ContentSecurityPolicy::New();
  csp->treat_as_public_address = true;
  response_head.parsed_headers = ParsedHeaders::New();
  response_head.parsed_headers->content_security_policy.push_back(
      std::move(csp));

  EXPECT_EQ(
      IPAddressSpace::kPublic,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceTest, CalculateResourceAddressSpaceFileURL) {
  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateResourceAddressSpace(GURL("file:///foo"), IPAddress()));
}

TEST(IPAddressSpaceTest, CalculateResourceAddressSpaceIPAddress) {
  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateResourceAddressSpace(GURL("http:///foo.test"),
                                          IPAddress::IPv4Localhost()));
  EXPECT_EQ(IPAddressSpace::kPrivate,
            CalculateResourceAddressSpace(GURL("http:///foo.test"),
                                          PrivateIPv4Address()));
  EXPECT_EQ(IPAddressSpace::kPublic,
            CalculateResourceAddressSpace(GURL("http:///foo.test"),
                                          PublicIPv4Address()));
  EXPECT_EQ(
      IPAddressSpace::kUnknown,
      CalculateResourceAddressSpace(GURL("http:///foo.test"), IPAddress()));
}

}  // namespace
}  // namespace blink
