// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include <utility>

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

using mojom::ContentSecurityPolicy;
using mojom::IPAddressSpace;
using mojom::ParsedHeaders;
using mojom::URLResponseHead;
using net::IPAddress;
using net::IPAddressBytes;
using net::IPEndPoint;

IPAddress PrivateIPv4Address() {
  return IPAddress(192, 168, 1, 1);
}

TEST(IPAddressSpaceTest, IPAddressToIPAddressSpacev4) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress()), IPAddressSpace::kUnknown);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(64, 233, 160, 0)),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPAddressToIPAddressSpace(PrivateIPv4Address()),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(10, 1, 1, 1)),
            IPAddressSpace::kPrivate);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv4Localhost()),
            IPAddressSpace::kLocal);
}

IPAddressBytes IPv6BytesWithPrefix(uint8_t prefix) {
  IPAddressBytes bytes;
  bytes.Resize(IPAddress::kIPv6AddressSize);
  bytes.data()[0] = prefix;
  return bytes;
}

TEST(IPAddressSpaceTest, IPAddressToAddressSpacev6) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(IPv6BytesWithPrefix(42))),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(IPv6BytesWithPrefix(0xfd))),
            IPAddressSpace::kPrivate);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLocal);
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanLocal) {
  EXPECT_FALSE(
      IsLessPublicAddressSpace(IPAddressSpace::kLocal, IPAddressSpace::kLocal));

  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kPrivate));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanPrivate) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                        IPAddressSpace::kPrivate));

  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                       IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                       IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanPublic) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kPrivate));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanUnknown) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kPrivate));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, CalculateClientAddressSpaceFileURL) {
  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateClientAddressSpace(GURL("file:///foo"), nullptr));
}

TEST(IPAddressSpaceTest,
     CalculateIPAddressSpaceFetchedViaServiceWorkerFromFile) {
  URLResponseHead response_head;
  response_head.url_list_via_service_worker.emplace_back("http://bar.test");
  response_head.url_list_via_service_worker.emplace_back("file:///foo");
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(
      IPAddressSpace::kLocal,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceTest,
     CalculateIPAddressSpaceFetchedViaServiceWorkerFromHttp) {
  URLResponseHead response_head;
  response_head.url_list_via_service_worker.emplace_back("file:///foo");
  response_head.url_list_via_service_worker.emplace_back("http://bar.test");
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(
      IPAddressSpace::kUnknown,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceTest,
     CalculateIPAddressSpaceFetchedViaServiceWorkerFromHttpInsteadOfFile) {
  URLResponseHead response_head;
  response_head.url_list_via_service_worker.emplace_back("http://bar.test");
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(IPAddressSpace::kUnknown,
            CalculateClientAddressSpace(GURL("file:///foo"), &response_head));
}

TEST(IPAddressSpaceTest, CalculateClientAddressSpaceNullResponseHead) {
  EXPECT_EQ(IPAddressSpace::kUnknown,
            CalculateClientAddressSpace(GURL("http://foo.test"), nullptr));
}

TEST(IPAddressSpaceTest, CalculateClientAddressSpaceEmptyResponseHead) {
  URLResponseHead response_head;
  response_head.parsed_headers = ParsedHeaders::New();
  EXPECT_EQ(
      IPAddressSpace::kUnknown,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceTest, CalculateClientAddressSpaceIPAddress) {
  URLResponseHead response_head;
  response_head.remote_endpoint = IPEndPoint(PrivateIPv4Address(), 1234);
  response_head.parsed_headers = ParsedHeaders::New();

  EXPECT_EQ(
      IPAddressSpace::kPrivate,
      CalculateClientAddressSpace(GURL("http://foo.test"), &response_head));
}

TEST(IPAddressSpaceTest, CalculateClientAddressSpaceTreatAsPublicAddress) {
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

}  // namespace
}  // namespace network
