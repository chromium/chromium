// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <ostream>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/errorcode.h"
#include "sandbox/linux/bpf_dsl/policy_compiler.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"

namespace sandbox {
namespace bpf_dsl {
namespace {

class ReturnResultExprImpl : public internal::ResultExprImpl {
 public:
  explicit ReturnResultExprImpl(uint32_t ret) : ret_(ret) {}

  ReturnResultExprImpl(const ReturnResultExprImpl&) = delete;
  ReturnResultExprImpl& operator=(const ReturnResultExprImpl&) = delete;

  ~ReturnResultExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc) const override {
    return pc->Return(ret_);
  }

  bool IsAllow() const override { return IsAction(SECCOMP_RET_ALLOW); }

  bool IsDeny() const override {
    return IsAction(SECCOMP_RET_ERRNO) || IsAction(SECCOMP_RET_KILL) ||
           IsAction(SECCOMP_RET_USER_NOTIF);
  }

 private:
  bool IsAction(uint32_t action) const {
    return (ret_ & SECCOMP_RET_ACTION) == action;
  }

  uint32_t ret_;
};

class TrapResultExprImpl : public internal::ResultExprImpl {
 public:
  TrapResultExprImpl(TrapRegistry::TrapFnc func, const void* arg, bool safe)
      : handler_(func, arg, safe) {}

  TrapResultExprImpl(const TrapResultExprImpl&) = delete;
  TrapResultExprImpl& operator=(const TrapResultExprImpl&) = delete;

  ~TrapResultExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc) const override {
    return pc->Trap(handler_);
  }

  bool HasUnsafeTraps() const override { return !handler_.safe; }
  bool IsDeny() const override { return true; }

 private:
  TrapRegistry::Handler handler_;
};

class IfThenResultExprImpl : public internal::ResultExprImpl {
 public:
  IfThenResultExprImpl(BoolExpr cond,
                       ResultExpr then_result,
                       ResultExpr else_result)
      : cond_(std::move(cond)),
        then_result_(std::move(then_result)),
        else_result_(std::move(else_result)) {}

  IfThenResultExprImpl(const IfThenResultExprImpl&) = delete;
  IfThenResultExprImpl& operator=(const IfThenResultExprImpl&) = delete;

  ~IfThenResultExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc) const override {
    // We compile the "then" and "else" expressions in separate statements so
    // they have a defined sequencing.  See https://crbug.com/529480.
    CodeGen::Node then_node = then_result_->Compile(pc);
    CodeGen::Node else_node = else_result_->Compile(pc);
    return cond_->Compile(pc, then_node, else_node);
  }

  bool HasUnsafeTraps() const override {
    return then_result_->HasUnsafeTraps() || else_result_->HasUnsafeTraps();
  }

 private:
  BoolExpr cond_;
  ResultExpr then_result_;
  ResultExpr else_result_;
};

class ConstBoolExprImpl : public internal::BoolExprImpl {
 public:
  explicit ConstBoolExprImpl(bool value) : value_(value) {}

  ConstBoolExprImpl(const ConstBoolExprImpl&) = delete;
  ConstBoolExprImpl& operator=(const ConstBoolExprImpl&) = delete;

  ~ConstBoolExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return value_ ? then_node : else_node;
  }

 private:
  bool value_;
};

class MaskedEqualBoolExprImpl : public internal::BoolExprImpl {
 public:
  MaskedEqualBoolExprImpl(int argno,
                          size_t width,
                          uint64_t mask,
                          uint64_t value)
      : argno_(argno), width_(width), mask_(mask), value_(value) {}

  MaskedEqualBoolExprImpl(const MaskedEqualBoolExprImpl&) = delete;
  MaskedEqualBoolExprImpl& operator=(const MaskedEqualBoolExprImpl&) = delete;

  ~MaskedEqualBoolExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return pc->MaskedEqual(argno_, width_, mask_, value_, then_node, else_node);
  }

 private:
  int argno_;
  size_t width_;
  uint64_t mask_;
  uint64_t value_;
};

class NegateBoolExprImpl : public internal::BoolExprImpl {
 public:
  explicit NegateBoolExprImpl(BoolExpr cond) : cond_(std::move(cond)) {}

  NegateBoolExprImpl(const NegateBoolExprImpl&) = delete;
  NegateBoolExprImpl& operator=(const NegateBoolExprImpl&) = delete;

  ~NegateBoolExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return cond_->Compile(pc, else_node, then_node);
  }

 private:
  BoolExpr cond_;
};

class AndBoolExprImpl : public internal::BoolExprImpl {
 public:
  AndBoolExprImpl(BoolExpr lhs, BoolExpr rhs)
      : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

  AndBoolExprImpl(const AndBoolExprImpl&) = delete;
  AndBoolExprImpl& operator=(const AndBoolExprImpl&) = delete;

  ~AndBoolExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return lhs_->Compile(pc, rhs_->Compile(pc, then_node, else_node),
                         else_node);
  }

 private:
  BoolExpr lhs_;
  BoolExpr rhs_;
};

class OrBoolExprImpl : public internal::BoolExprImpl {
 public:
  OrBoolExprImpl(BoolExpr lhs, BoolExpr rhs)
      : lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

  OrBoolExprImpl(const OrBoolExprImpl&) = delete;
  OrBoolExprImpl& operator=(const OrBoolExprImpl&) = delete;

  ~OrBoolExprImpl() override = default;

  CodeGen::Node Compile(PolicyCompiler* pc,
                        CodeGen::Node then_node,
                        CodeGen::Node else_node) const override {
    return lhs_->Compile(pc, then_node,
                         rhs_->Compile(pc, then_node, else_node));
  }

 private:
  BoolExpr lhs_;
  BoolExpr rhs_;
};

}  // namespace

namespace internal {

bool ResultExprImpl::HasUnsafeTraps() const {
  return false;
}

bool ResultExprImpl::IsAllow() const {
  return false;
}

bool ResultExprImpl::IsDeny() const {
  return false;
}

uint64_t DefaultMask(size_t size) {
  switch (size) {
    case 4:
      return std::numeric_limits<uint32_t>::max();
    case 8:
      return std::numeric_limits<uint64_t>::max();
    default:
      CHECK(false) << "Unimplemented DefaultMask case";
      return 0;
  }
}

BoolExpr ArgEq(int num, size_t size, uint64_t mask, uint64_t val) {
  // If this is changed, update Arg<T>::EqualTo's static_cast rules
  // accordingly.
  CHECK(size == 4 || size == 8);

  return std::make_shared<MaskedEqualBoolExprImpl>(num, size, mask, val);
}

}  // namespace internal

ResultExpr Allow() {
  return std::make_shared<ReturnResultExprImpl>(SECCOMP_RET_ALLOW);
}

ResultExpr Error(int err) {
  CHECK(err >= ErrorCode::ERR_MIN_ERRNO && err <= ErrorCode::ERR_MAX_ERRNO);
  return std::make_shared<ReturnResultExprImpl>(SECCOMP_RET_ERRNO + err);
}

ResultExpr Kill() {
  return std::make_shared<ReturnResultExprImpl>(SECCOMP_RET_KILL);
}

ResultExpr Trace(uint16_t aux) {
  return std::make_shared<ReturnResultExprImpl>(SECCOMP_RET_TRACE + aux);
}

ResultExpr Trap(TrapRegistry::TrapFnc trap_func, const void* aux) {
  return std::make_shared<TrapResultExprImpl>(trap_func, aux, true /* safe */);
}

ResultExpr UnsafeTrap(TrapRegistry::TrapFnc trap_func, const void* aux) {
  return std::make_shared<TrapResultExprImpl>(trap_func, aux,
                                              false /* unsafe */);
}

ResultExpr UserNotify() {
  return std::make_shared<ReturnResultExprImpl>(SECCOMP_RET_USER_NOTIF);
}

BoolExpr BoolConst(bool value) {
  return std::make_shared<ConstBoolExprImpl>(value);
}

BoolExpr Not(BoolExpr cond) {
  return std::make_shared<NegateBoolExprImpl>(std::move(cond));
}

BoolExpr AllOf() {
  return BoolConst(true);
}

BoolExpr AllOf(BoolExpr lhs, BoolExpr rhs) {
  return std::make_shared<AndBoolExprImpl>(std::move(lhs), std::move(rhs));
}

BoolExpr AnyOf() {
  return BoolConst(false);
}

BoolExpr AnyOf(BoolExpr lhs, BoolExpr rhs) {
  return std::make_shared<OrBoolExprImpl>(std::move(lhs), std::move(rhs));
}

Elser If(BoolExpr cond, ResultExpr then_result) {
  return Elser(nullptr).ElseIf(std::move(cond), std::move(then_result));
}

Elser::Elser(cons::List<Clause> clause_list) : clause_list_(clause_list) {
}

Elser::Elser(const Elser& elser) = default;

Elser::~Elser() = default;

Elser Elser::ElseIf(BoolExpr cond, ResultExpr then_result) const {
  return Elser(Cons(std::make_pair(std::move(cond), std::move(then_result)),
                    clause_list_));
}

ResultExpr Elser::Else(ResultExpr else_result) const {
  // We finally have the default result expression for this
  // if/then/else sequence.  Also, we've already accumulated all
  // if/then pairs into a list of reverse order (i.e., lower priority
  // conditions are listed before higher priority ones).  E.g., an
  // expression like
  //
  //    If(b1, e1).ElseIf(b2, e2).ElseIf(b3, e3).Else(e4)
  //
  // will have built up a list like
  //
  //    [(b3, e3), (b2, e2), (b1, e1)].
  //
  // Now that we have e4, we can walk the list and create a ResultExpr
  // tree like:
  //
  //    expr = e4
  //    expr = (b3 ? e3 : expr) = (b3 ? e3 : e4)
  //    expr = (b2 ? e2 : expr) = (b2 ? e2 : (b3 ? e3 : e4))
  //    expr = (b1 ? e1 : expr) = (b1 ? e1 : (b2 ? e2 : (b3 ? e3 : e4)))
  //
  // and end up with an appropriately chained tree.

  ResultExpr expr = std::move(else_result);
  for (const Clause& clause : clause_list_) {
    expr = std::make_shared<IfThenResultExprImpl>(clause.first, clause.second,
                                                  std::move(expr));
  }
  return expr;
}

}  // namespace bpf_dsl
}  // namespace sandbox

namespace std {
template class shared_ptr<const sandbox::bpf_dsl::internal::BoolExprImpl>;
template class shared_ptr<const sandbox::bpf_dsl::internal::ResultExprImpl>;
}  // namespace std
