// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/shared_buffer_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/writable_shared_memory_region.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/platform_shared_memory_mapping.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

// NOTE(vtl): There's currently not much to test for in
// |SharedBufferDispatcher::ValidateCreateOptions()|, but the tests should be
// expanded if/when options are added, so I've kept the general form of the
// tests from data_pipe_unittest.cc.

const uint32_t kSizeOfCreateOptions = sizeof(MojoCreateSharedBufferOptions);

// Does a cursory sanity check of |validated_options|. Calls
// |ValidateCreateOptions()| on already-validated options. The validated options
// should be valid, and the revalidated copy should be the same.
void RevalidateCreateOptions(
    const MojoCreateSharedBufferOptions& validated_options) {
  EXPECT_EQ(kSizeOfCreateOptions, validated_options.struct_size);
  // Nothing to check for flags.

  MojoCreateSharedBufferOptions revalidated_options = {};
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::ValidateCreateOptions(
                                &validated_options, &revalidated_options));
  EXPECT_EQ(validated_options.struct_size, revalidated_options.struct_size);
  EXPECT_EQ(validated_options.flags, revalidated_options.flags);
}

class SharedBufferDispatcherTest : public testing::Test {
 public:
  SharedBufferDispatcherTest() = default;

  SharedBufferDispatcherTest(const SharedBufferDispatcherTest&) = delete;
  SharedBufferDispatcherTest& operator=(const SharedBufferDispatcherTest&) =
      delete;

  ~SharedBufferDispatcherTest() override = default;
};

// Tests valid inputs to |ValidateCreateOptions()|.
TEST_F(SharedBufferDispatcherTest, ValidateCreateOptionsValid) {
  // Default options.
  {
    MojoCreateSharedBufferOptions validated_options = {};
    EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::ValidateCreateOptions(
                                  nullptr, &validated_options));
    RevalidateCreateOptions(validated_options);
  }

  // Different flags.
  MojoCreateSharedBufferFlags flags_values[] = {
      MOJO_CREATE_SHARED_BUFFER_FLAG_NONE};
  for (size_t i = 0; i < std::size(flags_values); i++) {
    const MojoCreateSharedBufferFlags flags = flags_values[i];

    // Different capacities (size 1).
    for (uint32_t capacity = 1; capacity <= 100 * 1000 * 1000; capacity *= 10) {
      MojoCreateSharedBufferOptions options = {
          kSizeOfCreateOptions,  // |struct_size|.
          flags                  // |flags|.
      };
      MojoCreateSharedBufferOptions validated_options = {};
      EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::ValidateCreateOptions(
                                    &options, &validated_options))
          << capacity;
      RevalidateCreateOptions(validated_options);
      EXPECT_EQ(options.flags, validated_options.flags);
    }
  }
}

TEST_F(SharedBufferDispatcherTest, ValidateCreateOptionsInvalid) {
  // Invalid |struct_size|.
  {
    MojoCreateSharedBufferOptions options = {
        1,                                   // |struct_size|.
        MOJO_CREATE_SHARED_BUFFER_FLAG_NONE  // |flags|.
    };
    MojoCreateSharedBufferOptions unused;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              SharedBufferDispatcher::ValidateCreateOptions(&options, &unused));
  }

  // Unknown |flags|.
  {
    MojoCreateSharedBufferOptions options = {
        kSizeOfCreateOptions,  // |struct_size|.
        ~0u                    // |flags|.
    };
    MojoCreateSharedBufferOptions unused;
    EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
              SharedBufferDispatcher::ValidateCreateOptions(&options, &unused));
  }
}

TEST_F(SharedBufferDispatcherTest, CreateAndMapBuffer) {
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher));
  ASSERT_TRUE(dispatcher);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER, dispatcher->GetType());

  // Make a couple of mappings.
  std::unique_ptr<PlatformSharedMemoryMapping> mapping1;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(0, 100, &mapping1));
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping1->GetBase());
  EXPECT_EQ(100u, mapping1->GetLength());
  // Write something.
  static_cast<char*>(mapping1->GetBase())[50] = 'x';

  std::unique_ptr<PlatformSharedMemoryMapping> mapping2;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(50, 50, &mapping2));
  ASSERT_TRUE(mapping2);
  ASSERT_TRUE(mapping2->GetBase());
  EXPECT_EQ(50u, mapping2->GetLength());
  EXPECT_EQ('x', static_cast<char*>(mapping2->GetBase())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());

  // Check that we can still read/write to mappings after the dispatcher has
  // gone away.
  static_cast<char*>(mapping2->GetBase())[1] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->GetBase())[51]);
}

TEST_F(SharedBufferDispatcherTest, CreateAndMapBufferFromPlatformBuffer) {
  base::WritableSharedMemoryRegion region =
      base::WritableSharedMemoryRegion::Create(100);
  ASSERT_TRUE(region.IsValid());
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK,
            SharedBufferDispatcher::CreateFromPlatformSharedMemoryRegion(
                base::WritableSharedMemoryRegion::TakeHandleForSerialization(
                    std::move(region)),
                &dispatcher));
  ASSERT_TRUE(dispatcher);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER, dispatcher->GetType());

  // Make a couple of mappings.
  std::unique_ptr<PlatformSharedMemoryMapping> mapping1;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(0, 100, &mapping1));
  ASSERT_TRUE(mapping1);
  ASSERT_TRUE(mapping1->GetBase());
  EXPECT_EQ(100u, mapping1->GetLength());
  // Write something.
  static_cast<char*>(mapping1->GetBase())[50] = 'x';

  std::unique_ptr<PlatformSharedMemoryMapping> mapping2;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->MapBuffer(50, 50, &mapping2));
  ASSERT_TRUE(mapping2);
  ASSERT_TRUE(mapping2->GetBase());
  EXPECT_EQ(50u, mapping2->GetLength());
  EXPECT_EQ('x', static_cast<char*>(mapping2->GetBase())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());

  // Check that we can still read/write to mappings after the dispatcher has
  // gone away.
  static_cast<char*>(mapping2->GetBase())[1] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->GetBase())[51]);
}

TEST_F(SharedBufferDispatcherTest, DuplicateBufferHandle) {
  scoped_refptr<SharedBufferDispatcher> dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher1));

  // Map and write something.
  std::unique_ptr<PlatformSharedMemoryMapping> mapping;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->MapBuffer(0, 100, &mapping));
  static_cast<char*>(mapping->GetBase())[0] = 'x';
  mapping.reset();

  // Duplicate |dispatcher1| and then close it.
  scoped_refptr<Dispatcher> dispatcher2;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher1->DuplicateBufferHandle(nullptr, &dispatcher2));
  ASSERT_TRUE(dispatcher2);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER, dispatcher2->GetType());

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->Close());

  // Map |dispatcher2| and read something.
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->MapBuffer(0, 100, &mapping));
  EXPECT_EQ('x', static_cast<char*>(mapping->GetBase())[0]);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->Close());
}

TEST_F(SharedBufferDispatcherTest, DuplicateBufferHandleOptionsValid) {
  scoped_refptr<SharedBufferDispatcher> dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher1));

  scoped_refptr<SharedBufferDispatcher> dispatcher2;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher2));

  MojoDuplicateBufferHandleOptions kReadOnlyOptions = {
      sizeof(MojoCreateSharedBufferOptions),
      MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY};

  // NOTE: We forbid handles from being duplicated read-only after they've been
  // duplicated non-read-only; conversely we also forbid handles from being
  // duplicated non-read-only after they've been duplicated read-only.
  scoped_refptr<Dispatcher> writable_duped_dispatcher1;
  scoped_refptr<Dispatcher> read_only_duped_dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->DuplicateBufferHandle(
                                nullptr, &writable_duped_dispatcher1));
  EXPECT_TRUE(writable_duped_dispatcher1);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER,
            writable_duped_dispatcher1->GetType());
  {
    std::unique_ptr<PlatformSharedMemoryMapping> mapping;
    EXPECT_EQ(MOJO_RESULT_OK,
              writable_duped_dispatcher1->MapBuffer(0, 100, &mapping));
  }
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            dispatcher1->DuplicateBufferHandle(&kReadOnlyOptions,
                                               &read_only_duped_dispatcher1));
  EXPECT_FALSE(read_only_duped_dispatcher1);

  scoped_refptr<Dispatcher> read_only_duped_dispatcher2;
  scoped_refptr<Dispatcher> writable_duped_dispatcher2;
  EXPECT_EQ(MOJO_RESULT_OK,
            dispatcher2->DuplicateBufferHandle(&kReadOnlyOptions,
                                               &read_only_duped_dispatcher2));
  EXPECT_TRUE(read_only_duped_dispatcher2);
  EXPECT_EQ(Dispatcher::Type::SHARED_BUFFER,
            read_only_duped_dispatcher2->GetType());
  {
    std::unique_ptr<PlatformSharedMemoryMapping> mapping;
    EXPECT_EQ(MOJO_RESULT_OK,
              read_only_duped_dispatcher2->MapBuffer(0, 100, &mapping));
  }
  EXPECT_EQ(
      MOJO_RESULT_FAILED_PRECONDITION,
      dispatcher2->DuplicateBufferHandle(nullptr, &writable_duped_dispatcher2));
  EXPECT_FALSE(writable_duped_dispatcher2);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->Close());
  EXPECT_EQ(MOJO_RESULT_OK, writable_duped_dispatcher1->Close());
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher2->Close());
  EXPECT_EQ(MOJO_RESULT_OK, read_only_duped_dispatcher2->Close());
}

TEST_F(SharedBufferDispatcherTest, DuplicateBufferHandleOptionsInvalid) {
  scoped_refptr<SharedBufferDispatcher> dispatcher1;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher1));

  // Invalid |struct_size|.
  {
    MojoDuplicateBufferHandleOptions options = {
        1u, MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_NONE};
    scoped_refptr<Dispatcher> dispatcher2;
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              dispatcher1->DuplicateBufferHandle(&options, &dispatcher2));
    EXPECT_FALSE(dispatcher2);
  }

  // Unknown |flags|.
  {
    MojoDuplicateBufferHandleOptions options = {
        sizeof(MojoDuplicateBufferHandleOptions), ~0u};
    scoped_refptr<Dispatcher> dispatcher2;
    EXPECT_EQ(MOJO_RESULT_UNIMPLEMENTED,
              dispatcher1->DuplicateBufferHandle(&options, &dispatcher2));
    EXPECT_FALSE(dispatcher2);
  }

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher1->Close());
}

TEST_F(SharedBufferDispatcherTest, CreateInvalidNumBytes) {
  // Size too big.
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            SharedBufferDispatcher::Create(
                SharedBufferDispatcher::kDefaultCreateOptions, nullptr,
                std::numeric_limits<uint64_t>::max(), &dispatcher));
  EXPECT_FALSE(dispatcher);

  // Zero size.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            SharedBufferDispatcher::Create(
                SharedBufferDispatcher::kDefaultCreateOptions, nullptr, 0,
                &dispatcher));
  EXPECT_FALSE(dispatcher);
}

TEST_F(SharedBufferDispatcherTest, MapBufferInvalidArguments) {
  scoped_refptr<SharedBufferDispatcher> dispatcher;
  EXPECT_EQ(MOJO_RESULT_OK, SharedBufferDispatcher::Create(
                                SharedBufferDispatcher::kDefaultCreateOptions,
                                nullptr, 100, &dispatcher));

  MojoSharedBufferInfo info = {sizeof(info), 0u};
  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->GetBufferInfo(&info));

  std::unique_ptr<PlatformSharedMemoryMapping> mapping;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(0, info.size + 1, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(1, info.size, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            dispatcher->MapBuffer(0, 0, &mapping));
  EXPECT_FALSE(mapping);

  EXPECT_EQ(MOJO_RESULT_OK, dispatcher->Close());
}

}  // namespace
}  // namespace core
}  // namespace mojo
