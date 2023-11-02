// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_BPF_DSL_IMPL_H_
#define SANDBOX_LINUX_BPF_DSL_BPF_DSL_IMPL_H_

#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {
class PolicyCompiler;

namespace internal {

// Internal interface implemented by BoolExpr implementations.
class BoolExprImpl {
 public:
  BoolExprImpl(const BoolExprImpl&) = delete;
  BoolExprImpl& operator=(const BoolExprImpl&) = delete;

  // Compile uses |pc| to emit a CodeGen::Node that conditionally continues
  // to either |then_node| or |false_node|, depending on whether the represented
  // boolean expression is true or false.
  virtual CodeGen::Node Compile(PolicyCompiler* pc,
                                CodeGen::Node then_node,
                                CodeGen::Node else_node) const = 0;

 protected:
  BoolExprImpl() {}
  virtual ~BoolExprImpl() {}
};

// Internal interface implemented by ResultExpr implementations.
class ResultExprImpl {
 public:
  ResultExprImpl(const ResultExprImpl&) = delete;
  ResultExprImpl& operator=(const ResultExprImpl&) = delete;

  // Compile uses |pc| to emit a CodeGen::Node that executes the
  // represented result expression.
  virtual CodeGen::Node Compile(PolicyCompiler* pc) const = 0;

  // HasUnsafeTraps returns whether the result expression is or recursively
  // contains an unsafe trap expression.
  virtual bool HasUnsafeTraps() const;

  // IsAllow returns whether the result expression is an "allow" result.
  virtual bool IsAllow() const;

  // IsAllow returns whether the result expression is a "deny" result.
  virtual bool IsDeny() const;

 protected:
  ResultExprImpl() {}
  virtual ~ResultExprImpl() {}
};

}  // namespace internal
}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_BPF_DSL_IMPL_H_
