//
//  Copyright 2019 The Abseil Authors.
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
//
// -----------------------------------------------------------------------------
// File: flag.h
// -----------------------------------------------------------------------------
//
// This header file defines the `absl::Flag<T>` type for holding command-line
// flag data, and abstractions to create, get and set such flag data.
//
// It is important to note that this type is **unspecified** (an implementation
// detail) and you do not construct or manipulate actual `absl::Flag<T>`
// instances. Instead, you define and declare flags using the
// `ABSL_FLAG()` and `ABSL_DECLARE_FLAG()` macros, and get and set flag values
// using the `absl::GetFlag()` and `absl::SetFlag()` functions.

#ifndef ABSL_FLAGS_FLAG_H_
#define ABSL_FLAGS_FLAG_H_

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/flags/config.h"
#include "absl/flags/declare.h"
#include "absl/flags/internal/commandlineflag.h"
#include "absl/flags/internal/flag.h"
#include "absl/flags/marshalling.h"

namespace absl {

// Flag
//
// An `absl::Flag` holds a command-line flag value, providing a runtime
// parameter to a binary. Such flags should be defined in the global namespace
// and (preferably) in the module containing the binary's `main()` function.
//
// You should not construct and cannot use the `absl::Flag` type directly;
// instead, you should declare flags using the `ABSL_DECLARE_FLAG()` macro
// within a header file, and define your flag using `ABSL_FLAG()` within your
// header's associated `.cc` file. Such flags will be named `FLAGS_name`.
//
// Example:
//
//    .h file
//
//      // Declares usage of a flag named "FLAGS_count"
//      ABSL_DECLARE_FLAG(int, count);
//
//    .cc file
//
//      // Defines a flag named "FLAGS_count" with a default `int` value of 0.
//      ABSL_FLAG(int, count, 0, "Count of items to process");
//
// No public methods of `absl::Flag<T>` are part of the Abseil Flags API.
template <typename T>
using Flag = flags_internal::Flag<T>;

// GetFlag()
//
// Returns the value (of type `T`) of an `absl::Flag<T>` instance, by value. Do
// not construct an `absl::Flag<T>` directly and call `absl::GetFlag()`;
// instead, refer to flag's constructed variable name (e.g. `FLAGS_name`).
// Because this function returns by value and not by reference, it is
// thread-safe, but note that the operation may be expensive; as a result, avoid
// `absl::GetFlag()` within any tight loops.
//
// Example:
//
//   // FLAGS_count is a Flag of type `int`
//   int my_count = absl::GetFlag(FLAGS_count);
//
//   // FLAGS_firstname is a Flag of type `std::string`
//   std::string first_name = absl::GetFlag(FLAGS_firstname);
template <typename T>
T GetFlag(const absl::Flag<T>& flag) {
#define ABSL_FLAGS_INTERNAL_LOCK_FREE_VALIDATE(BIT) \
  static_assert(                                    \
      !std::is_same<T, BIT>::value,                 \
      "Do not specify explicit template parameters to absl::GetFlag");
  ABSL_FLAGS_INTERNAL_FOR_EACH_LOCK_FREE(ABSL_FLAGS_INTERNAL_LOCK_FREE_VALIDATE)
#undef ABSL_FLAGS_INTERNAL_LOCK_FREE_VALIDATE

  return flag.Get();
}

// Overload for `GetFlag()` for types that support lock-free reads.
#define ABSL_FLAGS_INTERNAL_LOCK_FREE_EXPORT(T) \
  extern T GetFlag(const absl::Flag<T>& flag);
ABSL_FLAGS_INTERNAL_FOR_EACH_LOCK_FREE(ABSL_FLAGS_INTERNAL_LOCK_FREE_EXPORT)
#undef ABSL_FLAGS_INTERNAL_LOCK_FREE_EXPORT

// SetFlag()
//
// Sets the value of an `absl::Flag` to the value `v`. Do not construct an
// `absl::Flag<T>` directly and call `absl::SetFlag()`; instead, use the
// flag's variable name (e.g. `FLAGS_name`). This function is
// thread-safe, but is potentially expensive. Avoid setting flags in general,
// but especially within performance-critical code.
template <typename T>
void SetFlag(absl::Flag<T>* flag, const T& v) {
  flag->Set(v);
}

// Overload of `SetFlag()` to allow callers to pass in a value that is
// convertible to `T`. E.g., use this overload to pass a "const char*" when `T`
// is `std::string`.
template <typename T, typename V>
void SetFlag(absl::Flag<T>* flag, const V& v) {
  T value(v);
  flag->Set(value);
}

}  // namespace absl


// ABSL_FLAG()
//
// This macro defines an `absl::Flag<T>` instance of a specified type `T`:
//
//   ABSL_FLAG(T, name, default_value, help);
//
// where:
//
//   * `T` is a supported flag type (see the list of types in `marshalling.h`),
//   * `name` designates the name of the flag (as a global variable
//     `FLAGS_name`),
//   * `default_value` is an expression holding the default value for this flag
//     (which must be implicitly convertible to `T`),
//   * `help` is the help text, which can also be an expression.
//
// This macro expands to a flag named 'FLAGS_name' of type 'T':
//
//   absl::Flag<T> FLAGS_name = ...;
//
// Note that all such instances are created as global variables.
//
// For `ABSL_FLAG()` values that you wish to expose to other translation units,
// it is recommended to define those flags within the `.cc` file associated with
// the header where the flag is declared.
//
// Note: do not construct objects of type `absl::Flag<T>` directly. Only use the
// `ABSL_FLAG()` macro for such construction.
#define ABSL_FLAG(Type, name, default_value, help) \
  ABSL_FLAG_IMPL(Type, name, default_value, help)

// ABSL_FLAG().OnUpdate()
//
// Defines a flag of type `T` with a callback attached:
//
//   ABSL_FLAG(T, name, default_value, help).OnUpdate(callback);
//
// After any setting of the flag value, the callback will be called at least
// once. A rapid sequence of changes may be merged together into the same
// callback. No concurrent calls to the callback will be made for the same
// flag. Callbacks are allowed to read the current value of the flag but must
// not mutate that flag.
//
// The update mechanism guarantees "eventual consistency"; if the callback
// derives an auxiliary data structure from the flag value, it is guaranteed
// that eventually the flag value and the derived data structure will be
// consistent.
//
// Note: ABSL_FLAG.OnUpdate() does not have a public definition. Hence, this
// comment serves as its API documentation.


// -----------------------------------------------------------------------------
// Implementation details below this section
// -----------------------------------------------------------------------------

// ABSL_FLAG_IMPL macro definition conditional on ABSL_FLAGS_STRIP_NAMES

#if ABSL_FLAGS_STRIP_NAMES
#define ABSL_FLAG_IMPL_FLAGNAME(txt) ""
#define ABSL_FLAG_IMPL_FILENAME() ""
#define ABSL_FLAG_IMPL_REGISTRAR(T, flag) \
  absl::flags_internal::FlagRegistrar<T, false>(&flag)
#else
#define ABSL_FLAG_IMPL_FLAGNAME(txt) txt
#define ABSL_FLAG_IMPL_FILENAME() __FILE__
#define ABSL_FLAG_IMPL_REGISTRAR(T, flag) \
  absl::flags_internal::FlagRegistrar<T, true>(&flag)
#endif

// ABSL_FLAG_IMPL macro definition conditional on ABSL_FLAGS_STRIP_HELP

#if ABSL_FLAGS_STRIP_HELP
#define ABSL_FLAG_IMPL_FLAGHELP(txt) absl::flags_internal::kStrippedFlagHelp
#else
#define ABSL_FLAG_IMPL_FLAGHELP(txt) txt
#endif

#define ABSL_FLAG_IMPL_DECLARE_HELP_WRAPPER(name, txt) \
  static std::string AbslFlagsWrapHelp##name() {       \
    return ABSL_FLAG_IMPL_FLAGHELP(txt);               \
  }

#define ABSL_FLAG_IMPL_DECLARE_DEF_VAL_WRAPPER(name, Type, default_value)   \
  static void* AbslFlagsInitFlag##name() {                                  \
    return absl::flags_internal::MakeFromDefaultValue<Type>(default_value); \
  }

// ABSL_FLAG_IMPL
//
// Note: Name of registrar object is not arbitrary. It is used to "grab"
// global name for FLAGS_no<flag_name> symbol, thus preventing the possibility
// of defining two flags with names foo and nofoo.
#define ABSL_FLAG_IMPL(Type, name, default_value, help)             \
  namespace absl /* block flags in namespaces */ {}                 \
  ABSL_FLAG_IMPL_DECLARE_DEF_VAL_WRAPPER(name, Type, default_value) \
  ABSL_FLAG_IMPL_DECLARE_HELP_WRAPPER(name, help)                   \
  ABSL_CONST_INIT absl::Flag<Type> FLAGS_##name(                    \
      ABSL_FLAG_IMPL_FLAGNAME(#name), &AbslFlagsWrapHelp##name,     \
      ABSL_FLAG_IMPL_FILENAME(),                                    \
      &absl::flags_internal::FlagMarshallingOps<Type>,              \
      &AbslFlagsInitFlag##name);                                    \
  extern bool FLAGS_no##name;                                       \
  bool FLAGS_no##name = ABSL_FLAG_IMPL_REGISTRAR(Type, FLAGS_##name)

// ABSL_RETIRED_FLAG
//
// Designates the flag (which is usually pre-existing) as "retired." A retired
// flag is a flag that is now unused by the program, but may still be passed on
// the command line, usually by production scripts. A retired flag is ignored
// and code can't access it at runtime.
//
// This macro registers a retired flag with given name and type, with a name
// identical to the name of the original flag you are retiring. The retired
// flag's type can change over time, so that you can retire code to support a
// custom flag type.
//
// This macro has the same signature as `ABSL_FLAG`. To retire a flag, simply
// replace an `ABSL_FLAG` definition with `ABSL_RETIRED_FLAG`, leaving the
// arguments unchanged (unless of course you actually want to retire the flag
// type at this time as well).
//
// `default_value` is only used as a double check on the type. `explanation` is
// unused.
// TODO(rogeeff): Return an anonymous struct instead of bool, and place it into
// the unnamed namespace.
#define ABSL_RETIRED_FLAG(type, flagname, default_value, explanation) \
  ABSL_ATTRIBUTE_UNUSED static const bool ignored_##flagname =        \
      ([] { return type(default_value); },                            \
       absl::flags_internal::RetiredFlag<type>(#flagname))

#endif  // ABSL_FLAGS_FLAG_H_
