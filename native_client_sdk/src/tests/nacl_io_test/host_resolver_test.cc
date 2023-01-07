// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "fake_ppapi/fake_pepper_interface.h"
#include "gtest/gtest.h"
#include "nacl_io/kernel_intercept.h"

using namespace nacl_io;
using namespace sdk_util;

namespace {

class HostResolverTest : public ::testing::Test {
 public:
  HostResolverTest() {}

  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(NULL));
  }

  void TearDown() {
    ki_uninit();
  }
};

#define FAKE_HOSTNAME "example.com"
#define FAKE_IP 0x01020304

class FakeHostResolverTest : public ::testing::Test {
 public:
  FakeHostResolverTest() : fake_resolver_(NULL) {}

  void SetUp() {
    fake_resolver_ = static_cast<FakeHostResolverInterface*>(
        pepper_.GetHostResolverInterface());

    // Seed the fake resolver with some data
    fake_resolver_->fake_hostname = FAKE_HOSTNAME;
    AddFakeAddress(AF_INET);

    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init_interface(NULL, &pepper_));
  }

  void AddFakeAddress(int family) {
    if (family == AF_INET) {
      int address_count = fake_resolver_->fake_addresses_v4.size();
      // Each new address we add is FAKE_IP incremented by 1
      // each time to be unique.
      sockaddr_in fake_addr;
      fake_addr.sin_family = family;
      fake_addr.sin_addr.s_addr = htonl(FAKE_IP + address_count);
      fake_resolver_->fake_addresses_v4.push_back(fake_addr);
    } else if (family == AF_INET6) {
      sockaddr_in6 fake_addr;
      fake_addr.sin6_family = family;
      int address_count = fake_resolver_->fake_addresses_v6.size();
      for (uint8_t i = 0; i < 16; i++) {
        fake_addr.sin6_addr.s6_addr[i] = i + address_count;
      }
      fake_resolver_->fake_addresses_v6.push_back(fake_addr);
    }
  }

  void TearDown() {
    ki_uninit();
  }

 protected:
  FakePepperInterface pepper_;
  FakeHostResolverInterface* fake_resolver_;
};

}  // namespace

#define NULL_INFO ((struct addrinfo*)NULL)
#define NULL_ADDR ((struct sockaddr*)NULL)
#define NULL_HOST (static_cast<hostent*>(NULL))

TEST_F(HostResolverTest, Getaddrinfo_Numeric) {
  struct addrinfo* ai = NULL;
  struct sockaddr_in* in;
  struct addrinfo hints;

  // Numeric only
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  uint32_t expected_addr = htonl(0x01020304);
  ASSERT_EQ(0, ki_getaddrinfo("1.2.3.4", NULL, &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);
  ASSERT_NE(NULL_ADDR, ai->ai_addr);
  ASSERT_EQ(AF_INET, ai->ai_family);
  ASSERT_EQ(SOCK_STREAM, ai->ai_socktype);
  in = (struct sockaddr_in*)ai->ai_addr;
  ASSERT_EQ(expected_addr, in->sin_addr.s_addr);
  ASSERT_EQ(NULL_INFO, ai->ai_next);

  ki_freeaddrinfo(ai);
}

TEST_F(HostResolverTest, Getaddrinfo_NumericService) {
  struct addrinfo* ai = NULL;
  struct sockaddr_in* in;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  ASSERT_EQ(0, ki_getaddrinfo("1.2.3.4", "0", &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);
  ASSERT_NE(NULL_ADDR, ai->ai_addr);
  in = (struct sockaddr_in*)ai->ai_addr;
  uint16_t expected_port = htons(0);
  ASSERT_EQ(expected_port, in->sin_port);
  ASSERT_EQ(NULL_INFO, ai->ai_next);
  ki_freeaddrinfo(ai);

  ASSERT_EQ(0, ki_getaddrinfo("1.2.3.4", "65000", &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);
  ASSERT_NE(NULL_ADDR, ai->ai_addr);
  in = (struct sockaddr_in*)ai->ai_addr;
  expected_port = htons(65000);
  ASSERT_EQ(expected_port, in->sin_port);
  ASSERT_EQ(NULL_INFO, ai->ai_next);
  ki_freeaddrinfo(ai);
}

TEST_F(HostResolverTest, Getnameinfo_Numeric) {
  char host[64];
  char serv[64];

  // IPv4 host + service to strings.
  struct sockaddr_in in;

  memset(&in, 0, sizeof(in));
  memset(host, 0, sizeof(host));
  memset(serv, 0, sizeof(serv));
  in.sin_family = AF_INET;
  in.sin_port = ntohs(443);
  in.sin_addr.s_addr = ntohl(0x01020304);

  ASSERT_EQ(0, ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in),
                              sizeof(in), host, sizeof(host), serv,
                              sizeof(serv), NI_NUMERICSERV));
  ASSERT_STREQ(host, "1.2.3.4");
  ASSERT_STREQ(serv, "443");

  // IPv4 host only.
  memset(host, 0, sizeof(host));
  ASSERT_EQ(0,
            ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in), sizeof(in),
                           host, sizeof(host), NULL, 0, NI_NUMERICSERV));
  ASSERT_STREQ(host, "1.2.3.4");

  // IPv6 host + service.
  struct sockaddr_in6 in6;

  memset(&in6, 0, sizeof(in6));
  memset(host, 0, sizeof(host));
  memset(serv, 0, sizeof(serv));
  in6.sin6_family = AF_INET6;
  in6.sin6_port = ntohs(80);
  in6.sin6_addr.s6_addr[0] = 0xfe;
  in6.sin6_addr.s6_addr[1] = 0x80;
  in6.sin6_addr.s6_addr[12] = 0x05;
  in6.sin6_addr.s6_addr[13] = 0x06;
  in6.sin6_addr.s6_addr[14] = 0x07;
  in6.sin6_addr.s6_addr[15] = 0x08;

  ASSERT_EQ(0, ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in6),
                              sizeof(in6), host, sizeof(host), serv,
                              sizeof(serv), NI_NUMERICSERV));
  ASSERT_STREQ(host, "fe80::506:708");
  ASSERT_STREQ(serv, "80");

  // IPv6 service only.
  memset(serv, 0, sizeof(serv));
  ASSERT_EQ(
      0, ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in6), sizeof(in6),
                        NULL, 0, serv, sizeof(serv), NI_NUMERICSERV));
  ASSERT_STREQ(serv, "80");
}

TEST_F(HostResolverTest, Getnameinfo_ErrorHandling) {
  struct sockaddr_in in;
  char host[64];
  char serv[64];

  memset(&in, 0, sizeof(in));
  memset(host, 0, sizeof(host));
  memset(serv, 0, sizeof(serv));
  in.sin_family = AF_INET;
  in.sin_port = ntohs(443);
  in.sin_addr.s_addr = ntohl(0x01020304);

  // Bogus salen, hostlen, or servlen.
  ASSERT_EQ(EAI_FAMILY, ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in),
                                       sizeof(in) - 4, host, sizeof(host), serv,
                                       sizeof(serv), NI_NUMERICSERV));
  ASSERT_EQ(EAI_OVERFLOW,
            ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in), sizeof(in),
                           host, 7, serv, sizeof(serv), NI_NUMERICSERV));
  ASSERT_EQ(EAI_OVERFLOW,
            ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in), sizeof(in),
                           host, sizeof(host), serv, 3, NI_NUMERICSERV));

  // User insists on names, but we can only provide numbers.
  ASSERT_EQ(EAI_NONAME,
            ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in), sizeof(in),
                           host, sizeof(host), serv, 3, NI_NAMEREQD));

  // User forgot to pass a host or serv buffer.
  ASSERT_EQ(EAI_NONAME,
            ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&in), sizeof(in),
                           NULL, 0, NULL, 0, NI_NUMERICSERV));

  // Wrong socket type.
  struct sockaddr unix_sock;
  memset(&unix_sock, 0, sizeof(unix_sock));
  memset(host, 0, sizeof(host));
  memset(serv, 0, sizeof(serv));
  unix_sock.sa_family = AF_UNIX;
  ASSERT_EQ(EAI_FAMILY,
            ki_getnameinfo(reinterpret_cast<struct sockaddr*>(&unix_sock),
                           sizeof(unix_sock), host, sizeof(host), serv,
                           sizeof(serv), NI_NUMERICSERV));
  ASSERT_STREQ(host, "");
  ASSERT_STREQ(serv, "");
}

TEST_F(HostResolverTest, Getaddrinfo_MissingPPAPI) {
  // Verify that full lookups fail due to lack of PPAPI interfaces
  struct addrinfo* ai = NULL;
  ASSERT_EQ(EAI_SYSTEM, ki_getaddrinfo("google.com", NULL, NULL, &ai));
}

TEST_F(HostResolverTest, Getaddrinfo_Passive) {
  struct addrinfo* ai = NULL;
  struct sockaddr_in* in;
  struct sockaddr_in6* in6;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  uint32_t expected_port = htons(22);
  in_addr_t expected_addr = htonl(INADDR_ANY);
  in6_addr expected_addr6 = IN6ADDR_ANY_INIT;

  // AI_PASSIVE means that the returned address will be a wildcard
  // address suitable for binding and listening.  This should not
  // hit PPAPI at all, so we don't need fakes.
  hints.ai_family = AF_INET;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_socktype = SOCK_DGRAM;
  ASSERT_EQ(0, ki_getaddrinfo(NULL, "22", &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);
  ASSERT_NE(NULL_ADDR, ai->ai_addr);
  ASSERT_EQ(NULL_INFO, ai->ai_next);
  in = (struct sockaddr_in*)ai->ai_addr;
  ASSERT_EQ(expected_addr, in->sin_addr.s_addr);
  ASSERT_EQ(expected_port, in->sin_port);
  ASSERT_EQ(AF_INET, in->sin_family);
  ki_freeaddrinfo(ai);

  // Same test with AF_INET6
  hints.ai_family = AF_INET6;
  ASSERT_EQ(0, ki_getaddrinfo(NULL, "22", &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);
  ASSERT_NE(NULL_ADDR, ai->ai_addr);
  ASSERT_EQ(NULL_INFO, ai->ai_next);
  in6 = (struct sockaddr_in6*)ai->ai_addr;
  ASSERT_EQ(expected_port, in6->sin6_port);
  ASSERT_EQ(AF_INET6, in6->sin6_family);
  ASSERT_EQ(0, memcmp(in6->sin6_addr.s6_addr,
               &expected_addr6,
               sizeof(expected_addr6)));
  ki_freeaddrinfo(ai);
}

TEST_F(HostResolverTest, Getaddrinfo_Passive_Any) {
  // Similar to Getaddrinfo_Passive but don't set
  // ai_family in the hints, so we should get muplitple
  // results back for the different families.
  struct addrinfo* ai = NULL;
  struct addrinfo* ai_orig = NULL;
  struct sockaddr_in* in;
  struct sockaddr_in6* in6;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  uint32_t expected_port = htons(22);
  in_addr_t expected_addr = htonl(INADDR_ANY);
  in6_addr expected_addr6 = IN6ADDR_ANY_INIT;

  hints.ai_flags = AI_PASSIVE;
  hints.ai_socktype = SOCK_DGRAM;
  ASSERT_EQ(0, ki_getaddrinfo(NULL, "22", &hints, &ai));
  ai_orig = ai;
  ASSERT_NE(NULL_INFO, ai);
  int count = 0;
  bool got_v4 = false;
  bool got_v6 = false;
  while (ai) {
    ASSERT_NE(NULL_ADDR, ai->ai_addr);
    switch (ai->ai_addr->sa_family) {
      case AF_INET:
        in = (struct sockaddr_in*)ai->ai_addr;
        ASSERT_EQ(expected_port, in->sin_port);
        ASSERT_EQ(AF_INET, in->sin_family);
        ASSERT_EQ(expected_addr, in->sin_addr.s_addr);
        got_v4 = true;
        break;
      case AF_INET6:
        in6 = (struct sockaddr_in6*)ai->ai_addr;
        ASSERT_EQ(expected_port, in6->sin6_port);
        ASSERT_EQ(AF_INET6, in6->sin6_family);
        ASSERT_EQ(0, memcmp(in6->sin6_addr.s6_addr,
                            &expected_addr6,
                            sizeof(expected_addr6)));
        got_v6 = true;
        break;
      default:
        ASSERT_TRUE(false) << "Unknown address type: " << ai->ai_addr;
        break;
    }
    ai = ai->ai_next;
    count++;
  }

  ASSERT_EQ(2, count);
  ASSERT_TRUE(got_v4);
  ASSERT_TRUE(got_v6);

  ki_freeaddrinfo(ai_orig);
}

TEST_F(FakeHostResolverTest, Getaddrinfo_Lookup) {
  struct addrinfo* ai = NULL;
  struct sockaddr_in* in;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  in_addr_t expected_addr = htonl(FAKE_IP);

  // Lookup the fake hostname using getaddrinfo
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(0, ki_getaddrinfo(FAKE_HOSTNAME, NULL, &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);
  ASSERT_NE(NULL_ADDR, ai->ai_addr);
  ASSERT_EQ(AF_INET, ai->ai_family);
  ASSERT_EQ(SOCK_STREAM, ai->ai_socktype);
  in = (struct sockaddr_in*)ai->ai_addr;
  ASSERT_EQ(expected_addr, in->sin_addr.s_addr);
  ASSERT_EQ(NULL_INFO, ai->ai_next);

  ki_freeaddrinfo(ai);
}

TEST_F(FakeHostResolverTest, Getaddrinfo_Multi) {
  struct addrinfo* ai = NULL;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  // Add four fake address on top of the initial one
  // that the fixture creates.
  AddFakeAddress(AF_INET);
  AddFakeAddress(AF_INET);
  AddFakeAddress(AF_INET6);
  AddFakeAddress(AF_INET6);

  hints.ai_socktype = SOCK_STREAM;

  // First we test with AF_INET
  hints.ai_family = AF_INET;
  ASSERT_EQ(0, ki_getaddrinfo(FAKE_HOSTNAME, NULL, &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);

  // We expect to be returned 3 AF_INET address with
  // address FAKE_IP, FAKE_IP+1 and FAKE_IP+2, since that
  // is that the fake was seeded with.
  uint32_t expected_addr = htonl(FAKE_IP);
  int count = 0;
  struct addrinfo* current = ai;
  while (current != NULL) {
    ASSERT_NE(NULL_ADDR, current->ai_addr);
    ASSERT_EQ(AF_INET, current->ai_family);
    ASSERT_EQ(SOCK_STREAM, current->ai_socktype);
    sockaddr_in* in = (sockaddr_in*)current->ai_addr;
    ASSERT_EQ(expected_addr, in->sin_addr.s_addr);
    expected_addr += htonl(1);
    current = current->ai_next;
    count++;
  }
  ASSERT_EQ(3, count);
  ki_freeaddrinfo(ai);

  // Same test but with AF_INET6
  hints.ai_family = AF_INET6;
  ASSERT_EQ(0, ki_getaddrinfo(FAKE_HOSTNAME, NULL, &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);

  count = 0;
  current = ai;
  while (current != NULL) {
    ASSERT_NE(NULL_ADDR, current->ai_addr);
    ASSERT_EQ(AF_INET6, current->ai_family);
    ASSERT_EQ(SOCK_STREAM, current->ai_socktype);
    sockaddr_in6* in = (sockaddr_in6*)current->ai_addr;
    for (int i = 0; i < 16; i++) {
      ASSERT_EQ(i + count, in->sin6_addr.s6_addr[i]);
    }
    current = current->ai_next;
    count++;
  }
  ASSERT_EQ(2, count);
  ki_freeaddrinfo(ai);

  // Same test but with AF_UNSPEC.  Here we expect to get
  // 5 address back: 3 * v4 and 2 * v6.
  hints.ai_family = AF_UNSPEC;
  ASSERT_EQ(0, ki_getaddrinfo(FAKE_HOSTNAME, NULL, &hints, &ai));
  ASSERT_NE(NULL_INFO, ai);

  count = 0;
  current = ai;
  while (current != NULL) {
    ASSERT_NE(NULL_ADDR, ai->ai_addr);
    ASSERT_EQ(SOCK_STREAM, ai->ai_socktype);
    current = current->ai_next;
    count++;
  }
  ASSERT_EQ(5, count);

  ki_freeaddrinfo(ai);
}

TEST_F(FakeHostResolverTest, Gethostbyname) {
  hostent* host = ki_gethostbyname(FAKE_HOSTNAME);

  // Verify the returned hostent structure
  ASSERT_NE(NULL_HOST, host);
  ASSERT_EQ(AF_INET, host->h_addrtype);
  ASSERT_EQ(sizeof(in_addr_t), host->h_length);
  ASSERT_STREQ(FAKE_HOSTNAME, host->h_name);

  in_addr_t** addr_list = reinterpret_cast<in_addr_t**>(host->h_addr_list);
  ASSERT_NE(reinterpret_cast<in_addr_t**>(NULL), addr_list);
  ASSERT_EQ(NULL, addr_list[1]);
  in_addr_t expected_addr = htonl(FAKE_IP);
  ASSERT_EQ(expected_addr, *addr_list[0]);
  // Check that h_addr also matches as in some libc's it may be a separate
  // member.
  in_addr_t* first_addr = reinterpret_cast<in_addr_t*>(host->h_addr);
  ASSERT_EQ(expected_addr, *first_addr);
}

TEST_F(FakeHostResolverTest, Gethostbyname_Failure) {
  hostent* host = ki_gethostbyname("nosuchhost.com");
  ASSERT_EQ(NULL_HOST, host);
  ASSERT_EQ(HOST_NOT_FOUND, h_errno);
}

// Looking up purely numeric hostnames should work without PPAPI
// so we don't need the fakes for this test
TEST_F(HostResolverTest, Gethostbyname_Numeric) {
  struct hostent* host = ki_gethostbyname("8.8.8.8");

  // Verify the returned hostent structure
  ASSERT_NE(NULL_HOST, host);
  ASSERT_EQ(AF_INET, host->h_addrtype);
  ASSERT_EQ(sizeof(in_addr_t), host->h_length);
  ASSERT_STREQ("8.8.8.8", host->h_name);

  in_addr_t** addr_list = reinterpret_cast<in_addr_t**>(host->h_addr_list);
  ASSERT_NE(reinterpret_cast<in_addr_t**>(NULL), addr_list);
  ASSERT_EQ(NULL, addr_list[1]);
  ASSERT_EQ(inet_addr("8.8.8.8"), *addr_list[0]);
  // Check that h_addr also matches as in some libc's it may be a separate
  // member.
  in_addr_t* first_addr = reinterpret_cast<in_addr_t*>(host->h_addr);
  ASSERT_EQ(inet_addr("8.8.8.8"), *first_addr);
}

// These utility functions are only used for newlib (glibc provides its own
// implementations of these functions).
#if !defined(__GLIBC__)

TEST(SocketUtilityFunctions, Hstrerror) {
  EXPECT_STREQ("Unknown error in gethostbyname: 2718.", hstrerror(2718));
}

TEST(SocketUtilityFunctions, Gai_Strerror) {
  EXPECT_STREQ("Unknown error in getaddrinfo: 2719.", gai_strerror(2719));
}

#endif  // !defined(__GLIBC__)
