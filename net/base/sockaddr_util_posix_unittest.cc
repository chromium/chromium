// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sockaddr_util_posix.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "net/base/sockaddr_storage.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// The largest possible platform-dependent value. Subtracting one for an extra
// NUL character at the front or back.
constexpr size_t kMaxUnixAddressPath = sizeof(sockaddr_un::sun_path) - 1;

}  // namespace

TEST(FillUnixAddressTest, SimpleAddress) {
  SockaddrStorage storage;
  std::string path = "/tmp/socket/path";

  EXPECT_TRUE(
      FillUnixAddress(path, /*use_abstract_namespace=*/false, &storage));

  // |storage.addr_len| indicates the full size of the data in sockaddr_un.
  // The size is increased by one byte to include the string NUL terminator.
  EXPECT_EQ(path.size() + 1U + offsetof(struct sockaddr_un, sun_path),
            (unsigned int)storage.addr_len);

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(&storage.addr_storage);
  EXPECT_EQ(socket_addr->sun_family, AF_UNIX);

  // Implicit conversion to std::string for comparison is fine since the path
  // is always NUL terminated.
  EXPECT_EQ(socket_addr->sun_path, path);
}

TEST(FillUnixAddressTest, PathEmpty) {
  SockaddrStorage storage;
  std::string path = "";
  EXPECT_FALSE(
      FillUnixAddress(path, /*use_abstract_namespace=*/false, &storage));
}

TEST(FillUnixAddressTest, AddressMaxLength) {
  SockaddrStorage storage;
  std::string path(kMaxUnixAddressPath, '0');

  EXPECT_TRUE(
      FillUnixAddress(path, /*use_abstract_namespace=*/false, &storage));

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(&storage.addr_storage);
  EXPECT_EQ(socket_addr->sun_family, AF_UNIX);
  EXPECT_EQ(socket_addr->sun_path, path);
}

TEST(FillUnixAddressTest, AddressTooLong) {
  SockaddrStorage storage;
  std::string path(kMaxUnixAddressPath + 1, '0');

  EXPECT_FALSE(
      FillUnixAddress(path, /*use_abstract_namespace=*/false, &storage));
}

TEST(FillUnixAddressTest, AbstractLinuxAddress) {
  SockaddrStorage storage;
  std::string path(kMaxUnixAddressPath, '0');

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(FillUnixAddress(path, /*use_abstract_namespace=*/true, &storage));

  EXPECT_EQ(path.size() + 1U + offsetof(struct sockaddr_un, sun_path),
            static_cast<size_t>(storage.addr_len));

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(&storage.addr_storage);
  EXPECT_EQ(socket_addr->sun_family, AF_UNIX);

  // The path buffer is preceded by a NUL character for abstract Linux
  // addresses.
  EXPECT_EQ(socket_addr->sun_path[0], '\0');

  // The rest of the path. Note that `addr_len` has already been verified to be
  // correct, so only need to check the remaining path.size() characters of the
  // address struct, after the nul. Create a span and then a string_view from it
  // because span's constructor creates a span from the length of the array
  // that's passed in, while string_view's constructor will treat the input as
  // C-string and look for the terminating null at the end, and abstract Linux
  // paths are not guaranteed to be null terminated when in a `sockaddr_un`.
  std::string_view unix_path = base::as_string_view(
      base::span(socket_addr->sun_path).subspan(1u, path.size()));
  EXPECT_EQ(unix_path, path);
#else
  // Other platforms don't support the abstract Linux namespace.
  EXPECT_FALSE(
      FillUnixAddress(path, /*use_abstract_namespace=*/true, &storage));
#endif
}

}  // namespace net
