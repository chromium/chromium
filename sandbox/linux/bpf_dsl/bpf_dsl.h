// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_
#define SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/bpf_dsl/cons.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/sandbox_export.h"

// The sandbox::bpf_dsl namespace provides a domain-specific language
// to make writing BPF policies more expressive.  In general, the
// object types all have value semantics (i.e., they can be copied
// around, returned from or passed to function calls, etc. without any
// surprising side effects), though not all support assignment.
//
// An idiomatic and demonstrative (albeit silly) example of this API
// would be:
//
//   #include "sandbox/linux/bpf_dsl/bpf_dsl.h"
//
//   namespace dsl = sandbox::bpf_dsl;
//
//   class SillyPolicy : public dsl::Policy {
//    public:
//     SillyPolicy() = default;
//     SillyPolicy(const SillyPolicy&) = delete;
//     SillyPolicy& operator=(const SillyPolicy&) = delete;
//     ~SillyPolicy() override = default;
//
//     dsl::ResultExpr EvaluateSyscall(int sysno) const override {
//       if (sysno != __NR_fcntl)
//         return dsl::Allow();
//       dsl::Arg<int> fd(0), cmd(1);
//       dsl::Arg<unsigned long> flags(2);
//       constexpr uint64_t kGoodFlags = O_ACCMODE | O_NONBLOCK;
//       return dsl::If(dsl::AllOf(fd == 0,
//                                 cmd == F_SETFL,
//                                 (flags & ~kGoodFlags) == 0),
//                      dsl::Allow())
//           .dsl::ElseIf(dsl::AnyOf(cmd == F_DUPFD, cmd == F_DUPFD_CLOEXEC),
//                        dsl::Error(EMFILE))
//           .dsl::Else(dsl::Trap(SetFlagHandler, nullptr));
//     }
//   };
//
// More generally, the DSL currently supports the following grammar:
//
//   result = Allow() | Error(errno) | Kill() | Trace(aux)
//          | Trap(trap_func, aux) | UnsafeTrap(trap_func, aux)
//          | If(bool, result)[.ElseIf(bool, result)].Else(result)
//          | Switch(arg)[.Case(val, result)].Default(result)
//   bool   = BoolConst(boolean) | Not(bool) | AllOf(bool...) | AnyOf(bool...)
//          | arg == val | arg != val
//   arg    = Arg<T>(num) | arg & mask
//
// The semantics of each function and operator are intended to be
// intuitive, but are described in more detail below.
//
// (Credit to Sean Parent's "Inheritance is the Base Class of Evil"
// talk at Going Native 2013 for promoting value semantics via shared
// pointers to immutable state.)

namespace sandbox {
namespace bpf_dsl {

template <typename T>
class Caser;

class Elser;

// ResultExpr is an opaque reference to an immutable result expression tree.
using ResultExpr = std::shared_ptr<const internal::ResultExprImpl>;

// BoolExpr is an opaque reference to an immutable boolean expression tree.
using BoolExpr = std::shared_ptr<const internal::BoolExprImpl>;

// Allow specifies a result that the system call should be allowed to
// execute normally.
SANDBOX_EXPORT ResultExpr Allow();

// Error specifies a result that the system call should fail with
// error number |err|.  As a special case, Error(0) will result in the
// system call appearing to have succeeded, but without having any
// side effects.
SANDBOX_EXPORT ResultExpr Error(int err);

// Kill specifies a result to kill the process (task) immediately.
SANDBOX_EXPORT ResultExpr Kill();

// Trace specifies a result to notify a tracing process via the
// PTRACE_EVENT_SECCOMP event and allow it to change or skip the system call.
// The value of |aux| will be available to the tracer via PTRACE_GETEVENTMSG.
SANDBOX_EXPORT ResultExpr Trace(uint16_t aux);

// Trap specifies a result that the system call should be handled by
// trapping back into userspace and invoking |trap_func|, passing
// |aux| as the second parameter.
SANDBOX_EXPORT ResultExpr
    Trap(TrapRegistry::TrapFnc trap_func, const void* aux);

// UnsafeTrap is like Trap, except the policy is marked as "unsafe"
// and allowed to use SandboxSyscall to invoke any system call.
//
// NOTE: This feature, by definition, disables all security features of
//   the sandbox. It should never be used in production, but it can be
//   very useful to diagnose code that is incompatible with the sandbox.
//   If even a single system call returns "UnsafeTrap", the security of
//   entire sandbox should be considered compromised.
SANDBOX_EXPORT ResultExpr
    UnsafeTrap(TrapRegistry::TrapFnc trap_func, const void* aux);

// UserNotify specifies that the kernel shall notify a listening process that a
// syscall occurred. The listening process may perform the system call on
// behalf of the sandboxed process, or may instruct the sandboxed process to
// continue the system call.
SANDBOX_EXPORT ResultExpr UserNotify();

// BoolConst converts a bool value into a BoolExpr.
SANDBOX_EXPORT BoolExpr BoolConst(bool value);

// Not returns a BoolExpr representing the logical negation of |cond|.
SANDBOX_EXPORT BoolExpr Not(BoolExpr cond);

// AllOf returns a BoolExpr representing the logical conjunction ("and")
// of zero or more BoolExprs.
SANDBOX_EXPORT BoolExpr AllOf();
SANDBOX_EXPORT BoolExpr AllOf(BoolExpr lhs, BoolExpr rhs);
template <typename... Rest>
SANDBOX_EXPORT BoolExpr AllOf(BoolExpr first, Rest&&... rest);

// AnyOf returns a BoolExpr representing the logical disjunction ("or")
// of zero or more BoolExprs.
SANDBOX_EXPORT BoolExpr AnyOf();
SANDBOX_EXPORT BoolExpr AnyOf(BoolExpr lhs, BoolExpr rhs);
template <typename... Rest>
SANDBOX_EXPORT BoolExpr AnyOf(BoolExpr first, Rest&&... rest);

template <typename T>
class SANDBOX_EXPORT Arg {
 public:
  // Initializes the Arg to represent the |num|th system call
  // argument (indexed from 0), which is of type |T|.
  explicit Arg(int num);

  Arg(const Arg& arg) : num_(arg.num_), mask_(arg.mask_) {}

  Arg& operator=(const Arg&) = delete;

  // Returns an Arg representing the current argument, but after
  // bitwise-and'ing it with |rhs|.
  friend Arg operator&(const Arg& lhs, uint64_t rhs) {
    return Arg(lhs.num_, lhs.mask_ & rhs);
  }

  // Returns a boolean expression comparing whether the system call argument
  // (after applying any bitmasks, if appropriate) equals |rhs|.
  friend BoolExpr operator==(const Arg& lhs, T rhs) { return lhs.EqualTo(rhs); }

  // Returns a boolean expression comparing whether the system call argument
  // (after applying any bitmasks, if appropriate) does not equal |rhs|.
  friend BoolExpr operator!=(const Arg& lhs, T rhs) { return Not(lhs == rhs); }

 private:
  Arg(int num, uint64_t mask) : num_(num), mask_(mask) {}

  BoolExpr EqualTo(T val) const;

  int num_;
  uint64_t mask_;
};

// If begins a conditional result expression predicated on the
// specified boolean expression.
SANDBOX_EXPORT Elser If(BoolExpr cond, ResultExpr then_result);

class SANDBOX_EXPORT Elser {
 public:
  Elser(const Elser& elser);

  Elser& operator=(const Elser&) = delete;

  ~Elser();

  // ElseIf extends the conditional result expression with another
  // "if then" clause, predicated on the specified boolean expression.
  Elser ElseIf(BoolExpr cond, ResultExpr then_result) const;

  // Else terminates a conditional result expression using |else_result| as
  // the default fallback result expression.
  ResultExpr Else(ResultExpr else_result) const;

 private:
  using Clause = std::pair<BoolExpr, ResultExpr>;

  explicit Elser(cons::List<Clause> clause_list);

  cons::List<Clause> clause_list_;

  friend Elser If(BoolExpr, ResultExpr);
  template <typename T>
  friend Caser<T> Switch(const Arg<T>&);
};

// Switch begins a switch expression dispatched according to the
// specified argument value.
template <typename T>
SANDBOX_EXPORT Caser<T> Switch(const Arg<T>& arg);

template <typename T>
class SANDBOX_EXPORT Caser {
 public:
  Caser(const Caser<T>& caser) : arg_(caser.arg_), elser_(caser.elser_) {}

  Caser& operator=(const Caser&) = delete;

  ~Caser() = default;

  // Case adds a single-value "case" clause to the switch.
  Caser<T> Case(T value, ResultExpr result) const;

  // Cases adds a multiple-value "case" clause to the switch.
  Caser<T> Cases(std::initializer_list<T> values, ResultExpr result) const;

  // Terminate the switch with a "default" clause.
  ResultExpr Default(ResultExpr result) const;

 private:
  Caser(const Arg<T>& arg, Elser elser) : arg_(arg), elser_(elser) {}

  Arg<T> arg_;
  Elser elser_;

  template <typename U>
  friend Caser<U> Switch(const Arg<U>&);
};

// =====================================================================
// Official API ends here.
// =====================================================================

namespace internal {

// Make argument-dependent lookup work.  This is necessary because although
// BoolExpr is defined in bpf_dsl, since it's merely a typedef for
// scoped_refptr<const internal::BoolExplImpl>, argument-dependent lookup only
// searches the "internal" nested namespace.
using bpf_dsl::Not;
using bpf_dsl::AllOf;
using bpf_dsl::AnyOf;

// Returns a boolean expression that represents whether system call
// argument |num| of size |size| is equal to |val|, when masked
// according to |mask|.  Users should use the Arg template class below
// instead of using this API directly.
SANDBOX_EXPORT BoolExpr
    ArgEq(int num, size_t size, uint64_t mask, uint64_t val);

// Returns the default mask for a system call argument of the specified size.
SANDBOX_EXPORT uint64_t DefaultMask(size_t size);

}  // namespace internal

template <typename T>
Arg<T>::Arg(int num)
    : num_(num), mask_(internal::DefaultMask(sizeof(T))) {
}

// Definition requires ArgEq to have been declared.  Moved out-of-line
// to minimize how much internal clutter users have to ignore while
// reading the header documentation.
//
// Additionally, we use this helper member function to avoid linker errors
// caused by defining operator== out-of-line.  For a more detailed explanation,
// see http://www.parashift.com/c++-faq-lite/template-friends.html.
template <typename T>
BoolExpr Arg<T>::EqualTo(T val) const {
  if (sizeof(T) == 4) {
    // Prevent sign-extension of negative int32_t values.
    return internal::ArgEq(num_, sizeof(T), mask_, static_cast<uint32_t>(val));
  }
  return internal::ArgEq(num_, sizeof(T), mask_, static_cast<uint64_t>(val));
}

template <typename T>
SANDBOX_EXPORT Caser<T> Switch(const Arg<T>& arg) {
  return Caser<T>(arg, Elser(nullptr));
}

template <typename T>
Caser<T> Caser<T>::Case(T value, ResultExpr result) const {
  return Cases({value}, std::move(result));
}

template <typename T>
Caser<T> Caser<T>::Cases(std::initializer_list<T> values,
                         ResultExpr result) const {
  // Theoretically we could evaluate arg_ just once and emit a more efficient
  // dispatch table, but for now we simply translate into an equivalent
  // If/ElseIf/Else chain.

  BoolExpr values_expr(BoolConst(false));
  for (T value : values) {
    values_expr = AnyOf(values_expr, arg_ == value);
  }

  return Caser<T>(arg_,
                  elser_.ElseIf(std::move(values_expr), std::move(result)));
}

template <typename T>
ResultExpr Caser<T>::Default(ResultExpr result) const {
  return elser_.Else(std::move(result));
}

template <typename... Rest>
BoolExpr AllOf(BoolExpr first, Rest&&... rest) {
  return AllOf(std::move(first), AllOf(std::forward<Rest>(rest)...));
}

template <typename... Rest>
BoolExpr AnyOf(BoolExpr first, Rest&&... rest) {
  return AnyOf(std::move(first), AnyOf(std::forward<Rest>(rest)...));
}

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_
