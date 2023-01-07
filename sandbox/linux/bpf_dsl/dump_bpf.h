// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_DUMP_BPF_H_
#define SANDBOX_LINUX_BPF_DSL_DUMP_BPF_H_

#include <string>

#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {

class SANDBOX_EXPORT DumpBPF {
 public:
  // PrintProgram writes |program| in a human-readable format to stderr.
  static void PrintProgram(const CodeGen::Program& program);

  // StringPrintProgram writes |program| in a human-readable format to
  // a std::string.
  static std::string StringPrintProgram(const CodeGen::Program& program);
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_DUMP_BPF_H_
