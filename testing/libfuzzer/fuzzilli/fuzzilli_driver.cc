// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/threading/thread_restrictions.h"
#include "v8/src/fuzzilli/cov.h"

#define WEAK_SANCOV_DEF(return_type, name, ...)                           \
  extern "C" __attribute__((visibility("default"))) __attribute__((weak)) \
  return_type                                                             \
  name(__VA_ARGS__)

WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_cmp, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_cmp1, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_cmp2, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_cmp4, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_cmp8, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_const_cmp1, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_const_cmp2, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_const_cmp4, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_const_cmp8, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_switch, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_div4, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_div8, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_gep, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_pc_indir, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_8bit_counters_init, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_bool_flag_init, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_pcs_init, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_cfs_init, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_pc_guard, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_trace_pc_guard_init, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_load1, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_load2, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_load4, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_load8, void) {}
WEAK_SANCOV_DEF(void, __sanitizer_cov_load16, void) {}

// File descriptors used for communication with Fuzzilli.
constexpr base::PlatformFile kControlReadFd = 100;
constexpr base::PlatformFile kControlWriteFd = 101;
constexpr base::PlatformFile kDataReadFd = 102;

int LLVMFuzzerRunDriverImpl(int* argc,
                            char*** argv,
                            int (*UserCb)(const uint8_t* Data, size_t Size)) {
  // Allow blocking for the whole fuzzing session.
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Open files for communication with Fuzzilli.
  auto ctrl_read_file = base::File(base::ScopedPlatformFile(kControlReadFd));
  auto ctrl_write_file = base::File(base::ScopedPlatformFile(kControlWriteFd));
  auto data_read_file = base::File(base::ScopedPlatformFile(kDataReadFd));

  // Send the "HELO" message to Fuzzilli to establish communication.
  constexpr auto kHelloMessage = base::span_from_cstring("HELO");
  constexpr size_t kExpectedSize = kHelloMessage.size();
  static_assert(kExpectedSize == 4);

  ctrl_write_file.WriteAtCurrentPosAndCheck(base::as_bytes(kHelloMessage));
  char actual_magic[kExpectedSize] = {};
  ctrl_read_file.ReadAtCurrentPosAndCheck(
      base::as_writable_bytes(base::make_span(actual_magic)));

  CHECK(base::ranges::equal(kHelloMessage, actual_magic));

  while (true) {
    // Read the action message ("exec") from Fuzzilli.
    constexpr auto kExpectedAction = base::span_from_cstring("exec");
    uint8_t action_msg[kExpectedAction.size()];
    if (!ctrl_read_file.ReadAtCurrentPosAndCheck(base::make_span(action_msg)) ||
        !base::ranges::equal(kExpectedAction, action_msg)) {
      LOG(WARNING) << "Unexpected message from Fuzzilli: " << action_msg;
      return 0;
    }

    // Read the size of the JavaScript script from Fuzzilli.
    uint64_t script_size = 0;
    ctrl_read_file.ReadAtCurrentPosAndCheck(
        base::as_writable_bytes(base::span_from_ref(script_size)));

    // Read the JavaScript script from Fuzzilli.
    std::vector<uint8_t> buffer(script_size + 1);
    data_read_file.ReadAtCurrentPosAndCheck(
        base::make_span(buffer.data(), script_size));
    buffer[script_size] = 0;

    // Run the script:
    int status = UserCb(buffer.data(), script_size) ? 1 << 8 : 0;

    // Fuzzilli status is similar to the Linux return status. Lower 8 bits are
    // used for signals, and higher 8 bits for return code.
    ctrl_write_file.WriteAtCurrentPosAndCheck(base::byte_span_from_ref(status));

    // After every iteration, we reset the coverage edges so that we can mark
    // which edges are hit in the next iteration. This is needed by Fuzzilli
    // instrumentation.
    sanitizer_cov_reset_edgeguards();
  }
}

extern "C" int LLVMFuzzerRunDriver(int* argc,
                                   char*** argv,
                                   int (*UserCb)(const uint8_t* Data,
                                                 size_t Size)) {
  return LLVMFuzzerRunDriverImpl(argc, argv, UserCb);
}
