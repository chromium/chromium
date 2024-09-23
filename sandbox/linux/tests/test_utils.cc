// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/tests/test_utils.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/memory/page_size.h"
#include "base/posix/eintr_wrapper.h"

namespace sandbox {

bool TestUtils::CurrentProcessHasChildren() {
  siginfo_t process_info;
  int ret = HANDLE_EINTR(
      waitid(P_ALL, 0, &process_info, WEXITED | WNOHANG | WNOWAIT));
  if (-1 == ret) {
    PCHECK(ECHILD == errno);
    return false;
  } else {
    return true;
  }
}

void TestUtils::HandlePostForkReturn(pid_t pid) {
  const int kChildExitCode = 1;
  if (pid > 0) {
    int status = 0;
    PCHECK(pid == HANDLE_EINTR(waitpid(pid, &status, 0)));
    CHECK(WIFEXITED(status));
    CHECK_EQ(kChildExitCode, WEXITSTATUS(status));
  } else if (pid == 0) {
    _exit(kChildExitCode);
  }
}

void* TestUtils::MapPagesOrDie(size_t num_pages) {
  void* addr = mmap(nullptr, num_pages * base::GetPageSize(),
                    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  PCHECK(addr);
  return addr;
}

void TestUtils::MprotectLastPageOrDie(char* addr, size_t num_pages) {
  size_t last_page_offset = (num_pages - 1) * base::GetPageSize();
  PCHECK(mprotect(addr + last_page_offset, base::GetPageSize(), PROT_NONE) >=
         0);
}

}  // namespace sandbox
