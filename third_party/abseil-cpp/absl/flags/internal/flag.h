//
// Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_FLAGS_INTERNAL_FLAG_H_
#define ABSL_FLAGS_INTERNAL_FLAG_H_

#include "absl/flags/internal/commandlineflag.h"
#include "absl/flags/internal/registry.h"

namespace absl {
namespace flags_internal {

// This is "unspecified" implementation of absl::Flag<T> type.
template <typename T>
class Flag : public flags_internal::CommandLineFlag {
 public:
  constexpr Flag(const char* name, const flags_internal::HelpGenFunc help_gen,
                 const char* filename,
                 const flags_internal::FlagMarshallingOpFn marshalling_op_arg,
                 const flags_internal::InitialValGenFunc initial_value_gen)
      : flags_internal::CommandLineFlag(
            name, flags_internal::HelpText::FromFunctionPointer(help_gen),
            filename, &flags_internal::FlagOps<T>, marshalling_op_arg,
            initial_value_gen,
            /*retired_arg=*/false, /*def_arg=*/nullptr,
            /*cur_arg=*/nullptr) {}

  T Get() const {
    // Implementation notes:
    //
    // We are wrapping a union around the value of `T` to serve three purposes:
    //
    //  1. `U.value` has correct size and alignment for a value of type `T`
    //  2. The `U.value` constructor is not invoked since U's constructor does
    //  not
    //     do it explicitly.
    //  3. The `U.value` destructor is invoked since U's destructor does it
    //     explicitly. This makes `U` a kind of RAII wrapper around non default
    //     constructible value of T, which is destructed when we leave the
    //     scope. We do need to destroy U.value, which is constructed by
    //     CommandLineFlag::Read even though we left it in a moved-from state
    //     after std::move.
    //
    // All of this serves to avoid requiring `T` being default constructible.
    union U {
      T value;
      U() {}
      ~U() { value.~T(); }
    };
    U u;

    this->Read(&u.value, &flags_internal::FlagOps<T>);
    return std::move(u.value);
  }

  bool AtomicGet(T* v) const {
    const int64_t r = this->atomic.load(std::memory_order_acquire);
    if (r != flags_internal::CommandLineFlag::kAtomicInit) {
      memcpy(v, &r, sizeof(T));
      return true;
    }

    return false;
  }

  void Set(const T& v) { this->Write(&v, &flags_internal::FlagOps<T>); }
};

// This class facilitates Flag object registration and tail expression-based
// flag definition, for example:
// ABSL_FLAG(int, foo, 42, "Foo help").OnUpdate(NotifyFooWatcher);
template <typename T, bool do_register>
class FlagRegistrar {
 public:
  explicit FlagRegistrar(Flag<T>* flag) : flag_(flag) {
    if (do_register) flags_internal::RegisterCommandLineFlag(flag_);
  }

  FlagRegistrar& OnUpdate(flags_internal::FlagCallback cb) && {
    flag_->SetCallback(cb);
    return *this;
  }

  // Make the registrar "die" gracefully as a bool on a line where registration
  // happens. Registrar objects are intended to live only as temporary.
  operator bool() const { return true; }  // NOLINT

 private:
  Flag<T>* flag_;  // Flag being registered (not owned).
};

// This struct and corresponding overload to MakeDefaultValue are used to
// facilitate usage of {} as default value in ABSL_FLAG macro.
struct EmptyBraces {};

template <typename T>
T* MakeFromDefaultValue(T t) {
  return new T(std::move(t));
}

template <typename T>
T* MakeFromDefaultValue(EmptyBraces) {
  return new T;
}

}  // namespace flags_internal
}  // namespace absl

#endif  // ABSL_FLAGS_INTERNAL_FLAG_H_
