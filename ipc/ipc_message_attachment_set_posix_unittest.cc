// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is POSIX only.

#include "ipc/ipc_message_attachment_set.h"

#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/fdio/fdio.h>
#endif

namespace IPC {
namespace {

// Get a safe file descriptor for test purposes.
int GetSafeFd() {
#if BUILDFLAG(IS_FUCHSIA)
  return fdio_fd_create_null();
#else
  return open("/dev/null", O_RDONLY);
#endif
}

// Returns true if fd was already closed.  Closes fd if not closed.
bool VerifyClosed(int fd) {
  const int duped = HANDLE_EINTR(dup(fd));
  if (duped != -1) {
    EXPECT_NE(IGNORE_EINTR(close(duped)), -1);
    EXPECT_NE(IGNORE_EINTR(close(fd)), -1);
    return false;
  }
  return true;
}

int GetFdAt(MessageAttachmentSet* set, int id) {
  return static_cast<internal::PlatformFileAttachment&>(
             *set->GetAttachmentAt(id))
      .TakePlatformFile();
}

// The MessageAttachmentSet will try and close some of the descriptor numbers
// which we given it. This is the base descriptor value. It's great enough such
// that no real descriptor will accidentally be closed.
static const int kFDBase = 50000;

TEST(MessageAttachmentSet, BasicAdd) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  ASSERT_EQ(set->size(), 0u);
  ASSERT_TRUE(set->empty());
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));
  ASSERT_EQ(set->size(), 1u);
  ASSERT_TRUE(!set->empty());

  // Empties the set and stops a warning about deleting a set with unconsumed
  // descriptors
  set->CommitAllDescriptors();
}

TEST(MessageAttachmentSet, BasicAddAndClose) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  ASSERT_EQ(set->size(), 0u);
  ASSERT_TRUE(set->empty());
  const int fd = GetSafeFd();
  ASSERT_TRUE(set->AddAttachment(
      new internal::PlatformFileAttachment(base::ScopedFD(fd))));
  ASSERT_EQ(set->size(), 1u);
  ASSERT_TRUE(!set->empty());

  set->CommitAllDescriptors();

  ASSERT_TRUE(VerifyClosed(fd));
}
TEST(MessageAttachmentSet, MaxSize) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  for (size_t i = 0; i < MessageAttachmentSet::kMaxDescriptorsPerMessage; ++i)
    ASSERT_TRUE(set->AddAttachment(
        new internal::PlatformFileAttachment(kFDBase + 1 + i)));

  ASSERT_TRUE(
      !set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));

  set->CommitAllDescriptors();
}

TEST(MessageAttachmentSet, WalkInOrder) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 1)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 2)));

  ASSERT_EQ(GetFdAt(set.get(), 0), kFDBase);
  ASSERT_EQ(GetFdAt(set.get(), 1), kFDBase + 1);
  ASSERT_EQ(GetFdAt(set.get(), 2), kFDBase + 2);
  ASSERT_FALSE(set->GetAttachmentAt(0));

  set->CommitAllDescriptors();
}

TEST(MessageAttachmentSet, WalkWrongOrder) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 1)));
  ASSERT_TRUE(
      set->AddAttachment(new internal::PlatformFileAttachment(kFDBase + 2)));

  ASSERT_EQ(GetFdAt(set.get(), 0), kFDBase);
  ASSERT_FALSE(set->GetAttachmentAt(2));

  set->CommitAllDescriptors();
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DontClose DISABLED_DontClose
#else
#define MAYBE_DontClose DontClose
#endif
TEST(MessageAttachmentSet, MAYBE_DontClose) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  const int fd = GetSafeFd();
  ASSERT_TRUE(set->AddAttachment(new internal::PlatformFileAttachment(fd)));
  set->CommitAllDescriptors();

  ASSERT_FALSE(VerifyClosed(fd));
}

TEST(MessageAttachmentSet, DoClose) {
  scoped_refptr<MessageAttachmentSet> set(new MessageAttachmentSet);

  const int fd = GetSafeFd();
  ASSERT_TRUE(set->AddAttachment(
      new internal::PlatformFileAttachment(base::ScopedFD(fd))));
  set->CommitAllDescriptors();

  ASSERT_TRUE(VerifyClosed(fd));
}

}  // namespace
}  // namespace IPC
