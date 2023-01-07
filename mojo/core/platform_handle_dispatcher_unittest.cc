// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/platform_handle_dispatcher.h"

#include <stdio.h>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "mojo/core/test/test_utils.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

TEST(PlatformHandleDispatcherTest, Basic) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static const char kHelloWorld[] = "hello world";

  base::FilePath unused;
  base::ScopedFILE fp =
      CreateAndOpenTemporaryStreamInDir(temp_dir.GetPath(), &unused);
  ASSERT_TRUE(fp);
  EXPECT_EQ(sizeof(kHelloWorld),
            fwrite(kHelloWorld, 1, sizeof(kHelloWorld), fp.get()));

  PlatformHandle h = test::PlatformHandleFromFILE(std::move(fp));
  EXPECT_FALSE(fp);
  ASSERT_TRUE(h.is_valid());

  scoped_refptr<PlatformHandleDispatcher> dispatcher =
      PlatformHandleDispatcher::Create(std::move(h));
  EXPECT_FALSE(h.is_valid());
  EXPECT_EQ(Dispatcher::Type::PLATFORM_HANDLE, dispatcher->GetType());

  h = dispatcher->TakePlatformHandle();
  EXPECT_TRUE(h.is_valid());

  fp = test::FILEFromPlatformHandle(std::move(h), "rb");
  EXPECT_FALSE(h.is_valid());
  EXPECT_TRUE(fp);

  rewind(fp.get());
  char read_buffer[1000] = {};
  EXPECT_EQ(sizeof(kHelloWorld),
            fread(read_buffer, 1, sizeof(read_buffer), fp.get()));
  EXPECT_STREQ(kHelloWorld, read_buffer);

  // Try getting the handle again. (It should fail cleanly.)
  auto internal_handle = dispatcher->TakePlatformHandle();
  EXPECT_FALSE(internal_handle.is_valid());

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

TEST(PlatformHandleDispatcherTest, Serialization) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static const char kFooBar[] = "foo bar";

  base::FilePath unused;
  base::ScopedFILE fp =
      CreateAndOpenTemporaryStreamInDir(temp_dir.GetPath(), &unused);
  EXPECT_EQ(sizeof(kFooBar), fwrite(kFooBar, 1, sizeof(kFooBar), fp.get()));

  scoped_refptr<PlatformHandleDispatcher> dispatcher =
      PlatformHandleDispatcher::Create(
          test::PlatformHandleFromFILE(std::move(fp)));

  uint32_t num_bytes = 0;
  uint32_t num_ports = 0;
  uint32_t num_handles = 0;
  EXPECT_TRUE(dispatcher->BeginTransit());
  dispatcher->StartSerialize(&num_bytes, &num_ports, &num_handles);

  EXPECT_EQ(0u, num_bytes);
  EXPECT_EQ(0u, num_ports);
  EXPECT_EQ(1u, num_handles);

  PlatformHandle received_handle;
  EXPECT_TRUE(dispatcher->EndSerialize(nullptr, nullptr, &received_handle));

  dispatcher->CompleteTransitAndClose();

  EXPECT_TRUE(received_handle.is_valid());

  PlatformHandle handle = dispatcher->TakePlatformHandle();
  EXPECT_FALSE(handle.is_valid());

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, dispatcher->Close());

  dispatcher = static_cast<PlatformHandleDispatcher*>(
      Dispatcher::Deserialize(Dispatcher::Type::PLATFORM_HANDLE, nullptr,
                              num_bytes, nullptr, num_ports, &received_handle,
                              1u)
          .get());

  EXPECT_FALSE(received_handle.is_valid());
  EXPECT_TRUE(dispatcher->GetType() == Dispatcher::Type::PLATFORM_HANDLE);

  fp = test::FILEFromPlatformHandle(dispatcher->TakePlatformHandle(), "rb");
  EXPECT_TRUE(fp);

  rewind(fp.get());
  char read_buffer[1000] = {};
  EXPECT_EQ(sizeof(kFooBar),
            fread(read_buffer, 1, sizeof(read_buffer), fp.get()));
  EXPECT_STREQ(kFooBar, read_buffer);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

}  // namespace
}  // namespace core
}  // namespace mojo
