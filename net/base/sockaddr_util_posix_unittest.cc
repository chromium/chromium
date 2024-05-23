// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/sockaddr_util_posix.h"

#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "net/base/sockaddr_storage.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

size_t MaxPathLength(SockaddrStorage* storage) {
  // |storage.addr_len| is initialized to the largest possible platform-
  // dependent value. Subtracting the size of the initial fields in
  // sockaddr_un gives us the longest permissible path value including space
  // for an extra NUL character at the front or back.
  return storage->addr_len - offsetof(struct sockaddr_un, sun_path) - 1;
}

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
      reinterpret_cast<struct sockaddr_un*>(storage.addr);
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
  size_t path_max = MaxPathLength(&storage);
  std::string path(path_max, '0');

  EXPECT_TRUE(
      FillUnixAddress(path, /*use_abstract_namespace=*/false, &storage));

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(storage.addr);
  EXPECT_EQ(socket_addr->sun_family, AF_UNIX);
  EXPECT_EQ(socket_addr->sun_path, path);
}

TEST(FillUnixAddressTest, AddressTooLong) {
  SockaddrStorage storage;
  size_t path_max = MaxPathLength(&storage);
  std::string path(path_max + 1, '0');

  EXPECT_FALSE(
      FillUnixAddress(path, /*use_abstract_namespace=*/false, &storage));
}

TEST(FillUnixAddressTest, AbstractLinuxAddress) {
  SockaddrStorage storage;
  size_t path_max = MaxPathLength(&storage);
  std::string path(path_max, '0');

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(FillUnixAddress(path, /*use_abstract_namespace=*/true, &storage));

  EXPECT_EQ(path.size() + 1U + offsetof(struct sockaddr_un, sun_path),
            (unsigned int)storage.addr_len);

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(storage.addr);
  EXPECT_EQ(socket_addr->sun_family, AF_UNIX);

  // The path buffer is preceded by a NUL character for abstract Linux
  // addresses.
  EXPECT_EQ(socket_addr->sun_path[0], '\0');

  // The path string may not be NUL terminated, so do a buffer copy when
  // converting to std::string.
  std::string unix_path(reinterpret_cast<char*>(socket_addr->sun_path + 1),
                        path.size());
  EXPECT_EQ(unix_path, path);
#else
  // Other platforms don't support the abstract Linux namespace.
  EXPECT_FALSE(
      FillUnixAddress(path, /*use_abstract_namespace=*/true, &storage));
#endif
}

}  // namespace net
