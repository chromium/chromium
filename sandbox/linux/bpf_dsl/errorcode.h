// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_ERRORCODE_H__
#define SANDBOX_LINUX_BPF_DSL_ERRORCODE_H__

#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {

// TODO(mdempsky): Find a proper home for ERR_{MIN,MAX}_ERRNO and
// remove this header.
class SANDBOX_EXPORT ErrorCode {
 public:
  enum {
    ERR_MIN_ERRNO = 0,
#if defined(__mips__)
    // MIPS only supports errno up to 1133
    ERR_MAX_ERRNO = 1133,
#else
    // TODO(markus): Android only supports errno up to 255
    // (crbug.com/181647).
    ERR_MAX_ERRNO = 4095,
#endif
  };

  ErrorCode() = delete;
  ErrorCode(const ErrorCode&) = delete;
  ErrorCode& operator=(const ErrorCode&) = delete;
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_ERRORCODE_H__
