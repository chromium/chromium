// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/syscall_broker/remote_syscall_arg_handler.h"

#include <sys/mman.h>
#include <sys/types.h>

#include <algorithm>
#include <cstring>
#include <tuple>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/page_size.h"
#include "base/posix/unix_domain_socket.h"
#include "base/test/bind.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace syscall_broker {

namespace {
const char kPathPart[] = "/i/am/path";

void FillBufferWithPath(char* buf, size_t size, bool null_terminate) {
  SANDBOX_ASSERT_LE(size, static_cast<size_t>(PATH_MAX));
  size_t str_len = strlen(kPathPart);
  size_t len_left_to_write = size;
  char* curr_buf_pos = buf;
  while (len_left_to_write > 0) {
    size_t bytes_to_write = std::min(str_len, len_left_to_write);
    memcpy(curr_buf_pos, kPathPart, bytes_to_write);
    curr_buf_pos += bytes_to_write;
    len_left_to_write -= bytes_to_write;
  }

  if (null_terminate) {
    buf[size - 1] = '\0';
  }
}

void VerifyCorrectString(std::string str, size_t size) {
  SANDBOX_ASSERT_EQ(str.size(), size);
  size_t curr_path_part_pos = 0;
  for (char ch : str) {
    SANDBOX_ASSERT(ch == kPathPart[curr_path_part_pos]);
    curr_path_part_pos++;
    curr_path_part_pos %= strlen(kPathPart);
  }
}

pid_t ForkWaitingChild(base::OnceCallback<void(int)>
                           after_parent_signals_callback = base::DoNothing(),
                       base::ScopedFD* parent_sync_fd = nullptr) {
  base::ScopedFD parent_sync, child_sync;
  base::CreateSocketPair(&parent_sync, &child_sync);

  pid_t pid = fork();
  if (!pid) {
    parent_sync.reset();
    char dummy_char = 'a';
    std::vector<base::ScopedFD> empty_fd_vec;
    // Wait for parent to exit before exiting ourselves.
    base::UnixDomainSocket::RecvMsg(child_sync.get(), &dummy_char, 1,
                                    &empty_fd_vec);
    std::move(after_parent_signals_callback).Run(child_sync.get());
    _exit(1);
  }

  child_sync.reset();

  if (parent_sync_fd)
    *parent_sync_fd = std::move(parent_sync);
  else
    std::ignore = parent_sync.release();  // Closes when parent dies.
  return pid;
}

struct ReadTestConfig {
  size_t start_at = 0;
  size_t total_size = strlen(kPathPart) + 1;
  bool include_null_byte = true;
  bool last_page_inaccessible = false;
  RemoteProcessIOResult result = RemoteProcessIOResult::kSuccess;
};

void ReadTest(const ReadTestConfig& test_config) {
  // Map exactly the right number of pages for the config parameters.
  size_t total_pages = (test_config.start_at + test_config.total_size +
                        base::GetPageSize() - 1) /
                       base::GetPageSize();
  char* mmap_addr = static_cast<char*>(TestUtils::MapPagesOrDie(total_pages));
  char* addr = mmap_addr + test_config.start_at;
  FillBufferWithPath(addr, test_config.total_size,
                     test_config.include_null_byte);

  if (test_config.last_page_inaccessible)
    TestUtils::MprotectLastPageOrDie(mmap_addr, total_pages);

  pid_t pid = ForkWaitingChild();
  munmap(mmap_addr, base::GetPageSize() * total_pages);

  std::string out_str;
  SANDBOX_ASSERT_EQ(ReadFilePathFromRemoteProcess(pid, addr, &out_str),
                    test_config.result);
  if (test_config.result == RemoteProcessIOResult::kSuccess) {
    VerifyCorrectString(std::move(out_str), test_config.total_size - 1);
  }
}
}  // namespace

// | path + null_byte |
SANDBOX_TEST(BrokerRemoteSyscallArgHandler, BasicRead) {
  ReadTest(ReadTestConfig());
}

// | zero + path... | ...path + null_byte + zero |
SANDBOX_TEST(BrokerRemoteSyscallArgHandler, MultipageRead) {
  ReadTestConfig config;
  CHECK(PATH_MAX / 2 <= base::GetPageSize());
  config.start_at = base::GetPageSize() - (PATH_MAX / 2);
  config.total_size = PATH_MAX;

  ReadTest(config);
}

// | path... | ...path |
SANDBOX_TEST_ALLOW_NOISE(BrokerRemoteSyscallArgHandler, ReadExceededPathMax) {
  ReadTestConfig config;
  config.total_size = PATH_MAX * 2;
  config.result = RemoteProcessIOResult::kExceededPathMax;
}
// | path... | null_byte + zero |
SANDBOX_TEST_ALLOW_NOISE(BrokerRemoteSyscallArgHandler,
                         ReadBarelyExceededPathMax) {
  ReadTestConfig config;
  config.total_size = PATH_MAX + 1;
  config.result = RemoteProcessIOResult::kExceededPathMax;
}

// | zero + path... | INACCESSIBLE |
SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadUnreadablePage) {
  ReadTestConfig config;
  config.start_at = base::GetPageSize() - (PATH_MAX / 2);
  config.total_size = PATH_MAX / 2;
  config.last_page_inaccessible = true;
  config.include_null_byte = false;
  config.result = RemoteProcessIOResult::kRemoteMemoryInvalid;

  ReadTest(config);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadChunkMinus1) {
  ReadTestConfig config;
  config.total_size = internal::kNumBytesPerChunk - 1;

  ReadTest(config);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadChunk) {
  ReadTestConfig config;
  config.total_size = internal::kNumBytesPerChunk;

  ReadTest(config);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadChunkPlus1) {
  ReadTestConfig config;
  config.total_size = internal::kNumBytesPerChunk + 1;

  ReadTest(config);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadChunkEndingAtPage) {
  ReadTestConfig config;
  config.start_at = base::GetPageSize() - internal::kNumBytesPerChunk;
  config.total_size = internal::kNumBytesPerChunk;

  ReadTest(config);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadChunkEndingOnePastPage) {
  ReadTestConfig config;
  config.start_at = base::GetPageSize() - internal::kNumBytesPerChunk + 1;
  config.total_size = internal::kNumBytesPerChunk;

  ReadTest(config);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadChunkPlus1EndingOnePastPage) {
  ReadTestConfig config;
  config.start_at = base::GetPageSize() - internal::kNumBytesPerChunk;
  config.total_size = internal::kNumBytesPerChunk + 1;

  ReadTest(config);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, ReadChildExited) {
  void* addr = TestUtils::MapPagesOrDie(1);
  FillBufferWithPath(static_cast<char*>(addr), strlen(kPathPart) + 1, true);

  base::ScopedFD parent_sync, child_sync;
  base::CreateSocketPair(&parent_sync, &child_sync);

  pid_t pid = fork();
  if (!pid) {
    parent_sync.reset();
    _exit(1);
  }

  child_sync.reset();

  // Wait for child to exit before reading memory.
  char dummy_char = 'a';
  std::vector<base::ScopedFD> empty_fd_vec;
  base::UnixDomainSocket::RecvMsg(parent_sync.get(), &dummy_char, 1,
                                  &empty_fd_vec);

  munmap(addr, base::GetPageSize());

  std::string out_str;
  SANDBOX_ASSERT_EQ(ReadFilePathFromRemoteProcess(pid, addr, &out_str),
                    RemoteProcessIOResult::kRemoteExited);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, BasicWrite) {
  void* read_from = TestUtils::MapPagesOrDie(1);
  const size_t write_size = base::GetPageSize();
  FillBufferWithPath(static_cast<char*>(read_from), write_size, false);
  char* write_to = static_cast<char*>(TestUtils::MapPagesOrDie(1));
  base::ScopedFD parent_signal_fd;
  const std::vector<int> empty_fd_vec;

  pid_t pid =
      ForkWaitingChild(base::BindLambdaForTesting([=](int child_sync_fd) {
                         // Check correct result received and tell parent about
                         // success.
                         int res = memcmp(read_from, write_to, write_size);

                         base::UnixDomainSocket::SendMsg(
                             child_sync_fd, &res, sizeof(res), empty_fd_vec);
                         _exit(1);
                       }),
                       &parent_signal_fd);

  RemoteProcessIOResult result = WriteRemoteData(
      pid, reinterpret_cast<uintptr_t>(write_to), write_size,
      base::span<char>(static_cast<char*>(read_from), write_size));
  SANDBOX_ASSERT_EQ(result, RemoteProcessIOResult::kSuccess);

  // Release child.
  char dummy_char = 'a';
  base::UnixDomainSocket::SendMsg(parent_signal_fd.get(), &dummy_char, 1,
                                  empty_fd_vec);

  // Read result of memcmp and assert.
  int memcmp_res;
  std::vector<base::ScopedFD> dummy_fd_vec;
  base::UnixDomainSocket::RecvMsg(parent_signal_fd.get(), &memcmp_res,
                                  sizeof(memcmp_res), &dummy_fd_vec);
  SANDBOX_ASSERT_EQ(memcmp_res, 0);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, WriteToInvalidAddress) {
  char* write_to = static_cast<char*>(TestUtils::MapPagesOrDie(1));
  TestUtils::MprotectLastPageOrDie(write_to, 1);
  base::ScopedFD parent_signal_fd;
  const std::vector<int> empty_fd_vec;

  pid_t pid = ForkWaitingChild();
  munmap(write_to, base::GetPageSize());

  char buf[5];
  memset(buf, 'a', sizeof(buf));
  RemoteProcessIOResult result =
      WriteRemoteData(pid, reinterpret_cast<uintptr_t>(write_to), sizeof(buf),
                      base::span<char>(buf, sizeof(buf)));
  SANDBOX_ASSERT_EQ(result, RemoteProcessIOResult::kRemoteMemoryInvalid);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, WritePartiallyToInvalidAddress) {
  char* read_from = static_cast<char*>(TestUtils::MapPagesOrDie(2));
  const size_t write_size = base::GetPageSize();
  FillBufferWithPath(static_cast<char*>(read_from), write_size, false);
  char* write_to = static_cast<char*>(TestUtils::MapPagesOrDie(2));
  TestUtils::MprotectLastPageOrDie(write_to, 2);
  write_to += base::GetPageSize() / 2;
  base::ScopedFD parent_signal_fd;
  const std::vector<int> empty_fd_vec;

  pid_t pid = ForkWaitingChild();
  munmap(write_to, base::GetPageSize());

  RemoteProcessIOResult result =
      WriteRemoteData(pid, reinterpret_cast<uintptr_t>(write_to), write_size,
                      base::span<char>(read_from, write_size));
  SANDBOX_ASSERT_EQ(result, RemoteProcessIOResult::kRemoteMemoryInvalid);
}

SANDBOX_TEST(BrokerRemoteSyscallArgHandler, WriteChildExited) {
  char* addr = static_cast<char*>(TestUtils::MapPagesOrDie(1));
  FillBufferWithPath(static_cast<char*>(addr), strlen(kPathPart) + 1, true);

  base::ScopedFD parent_sync, child_sync;
  base::CreateSocketPair(&parent_sync, &child_sync);

  pid_t pid = fork();
  if (!pid) {
    parent_sync.reset();
    _exit(1);
  }

  child_sync.reset();

  // Wait for child to exit before writing memory.
  char dummy_char = 'a';
  std::vector<base::ScopedFD> empty_fd_vec;
  base::UnixDomainSocket::RecvMsg(parent_sync.get(), &dummy_char, 1,
                                  &empty_fd_vec);

  std::string out_str;
  SANDBOX_ASSERT_EQ(
      WriteRemoteData(pid, reinterpret_cast<uintptr_t>(addr), strlen(kPathPart),
                      base::span<char>(addr, strlen(kPathPart))),
      RemoteProcessIOResult::kRemoteExited);
}

}  // namespace syscall_broker
}  // namespace sandbox
