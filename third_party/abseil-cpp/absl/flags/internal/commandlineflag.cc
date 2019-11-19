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

#include "absl/flags/internal/commandlineflag.h"

#include <cassert>

#include "absl/base/internal/raw_logging.h"
#include "absl/base/optimization.h"
#include "absl/flags/config.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"

namespace absl {
namespace flags_internal {

// The help message indicating that the commandline flag has been
// 'stripped'. It will not show up when doing "-help" and its
// variants. The flag is stripped if ABSL_FLAGS_STRIP_HELP is set to 1
// before including absl/flags/flag.h

// This is used by this file, and also in commandlineflags_reporting.cc
const char kStrippedFlagHelp[] = "\001\002\003\004 (unknown) \004\003\002\001";

namespace {

// Currently we only validate flag values for user-defined flag types.
bool ShouldValidateFlagValue(const CommandLineFlag& flag) {
#define DONT_VALIDATE(T) \
  if (flag.IsOfType<T>()) return false;
  ABSL_FLAGS_INTERNAL_FOR_EACH_LOCK_FREE(DONT_VALIDATE)
  DONT_VALIDATE(std::string)
  DONT_VALIDATE(std::vector<std::string>)
#undef DONT_VALIDATE

  return true;
}

}  // namespace

absl::Mutex* InitFlag(CommandLineFlag* flag) {
  ABSL_CONST_INIT static absl::Mutex init_lock(absl::kConstInit);
  absl::Mutex* mu;

  {
    absl::MutexLock lock(&init_lock);

    if (flag->locks == nullptr) {  // Must initialize Mutexes for this flag.
      flag->locks = new flags_internal::CommandLineFlagLocks;
    }

    mu = &flag->locks->primary_mu;
  }

  {
    absl::MutexLock lock(mu);

    if (!flag->retired && flag->def == nullptr) {
      // Need to initialize def and cur fields.
      flag->def = (*flag->make_init_value)();
      flag->cur = Clone(flag->op, flag->def);
      UpdateCopy(flag);
      flag->inited.store(true, std::memory_order_release);
      flag->InvokeCallback();
    }
  }

  flag->inited.store(true, std::memory_order_release);
  return mu;
}

// Ensure that the lazily initialized fields of *flag have been initialized,
// and return &flag->locks->primary_mu.
absl::Mutex* CommandLineFlag::InitFlagIfNecessary() const
    ABSL_LOCK_RETURNED(locks->primary_mu) {
  if (!this->inited.load(std::memory_order_acquire)) {
    return InitFlag(const_cast<CommandLineFlag*>(this));
  }

  // All fields initialized; this->locks is therefore safe to read.
  return &this->locks->primary_mu;
}

void CommandLineFlag::Destroy() const {
  // Values are heap allocated for retired and Abseil Flags.
  if (IsRetired() || IsAbseilFlag()) {
    if (this->cur) Delete(this->op, this->cur);
    if (this->def) Delete(this->op, this->def);
  }

  delete this->locks;
}

bool CommandLineFlag::IsModified() const {
  absl::MutexLock l(InitFlagIfNecessary());
  return modified;
}

void CommandLineFlag::SetModified(bool is_modified) {
  absl::MutexLock l(InitFlagIfNecessary());
  modified = is_modified;
}

bool CommandLineFlag::IsSpecifiedOnCommandLine() const {
  absl::MutexLock l(InitFlagIfNecessary());
  return on_command_line;
}

absl::string_view CommandLineFlag::Typename() const {
  // We do not store/report type in Abseil Flags, so that user do not rely on in
  // at runtime
  if (IsAbseilFlag() || IsRetired()) return "";

#define HANDLE_V1_BUILTIN_TYPE(t) \
  if (IsOfType<t>()) {            \
    return #t;                    \
  }

  HANDLE_V1_BUILTIN_TYPE(bool);
  HANDLE_V1_BUILTIN_TYPE(int32_t);
  HANDLE_V1_BUILTIN_TYPE(int64_t);
  HANDLE_V1_BUILTIN_TYPE(uint64_t);
  HANDLE_V1_BUILTIN_TYPE(double);
#undef HANDLE_V1_BUILTIN_TYPE

  if (IsOfType<std::string>()) {
    return "string";
  }

  return "";
}

std::string CommandLineFlag::Filename() const {
  return flags_internal::GetUsageConfig().normalize_filename(this->filename);
}

std::string CommandLineFlag::DefaultValue() const {
  absl::MutexLock l(InitFlagIfNecessary());

  return Unparse(this->marshalling_op, this->def);
}

std::string CommandLineFlag::CurrentValue() const {
  absl::MutexLock l(InitFlagIfNecessary());

  return Unparse(this->marshalling_op, this->cur);
}

bool CommandLineFlag::HasValidatorFn() const {
  absl::MutexLock l(InitFlagIfNecessary());

  return this->validator != nullptr;
}

bool CommandLineFlag::SetValidatorFn(FlagValidator fn) {
  absl::MutexLock l(InitFlagIfNecessary());

  // ok to register the same function over and over again
  if (fn == this->validator) return true;

  // Can't set validator to a different function, unless reset first.
  if (fn != nullptr && this->validator != nullptr) {
    ABSL_INTERNAL_LOG(
        WARNING, absl::StrCat("Ignoring SetValidatorFn() for flag '", Name(),
                              "': validate-fn already registered"));

    return false;
  }

  this->validator = fn;
  return true;
}

bool CommandLineFlag::InvokeValidator(const void* value) const
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(this->locks->primary_mu) {
  if (!this->validator) {
    return true;
  }

  (void)value;

  ABSL_INTERNAL_LOG(
      FATAL,
      absl::StrCat("Flag '", Name(),
                   "' of encapsulated type should not have a validator"));

  return false;
}

void CommandLineFlag::SetCallback(
    const flags_internal::FlagCallback mutation_callback) {
  absl::MutexLock l(InitFlagIfNecessary());

  callback = mutation_callback;

  InvokeCallback();
}

// If the flag has a mutation callback this function invokes it. While the
// callback is being invoked the primary flag's mutex is unlocked and it is
// re-locked back after call to callback is completed. Callback invocation is
// guarded by flag's secondary mutex instead which prevents concurrent callback
// invocation. Note that it is possible for other thread to grab the primary
// lock and update flag's value at any time during the callback invocation.
// This is by design. Callback can get a value of the flag if necessary, but it
// might be different from the value initiated the callback and it also can be
// different by the time the callback invocation is completed.
// Requires that *primary_lock be held in exclusive mode; it may be released
// and reacquired by the implementation.
void CommandLineFlag::InvokeCallback()
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(this->locks->primary_mu) {
  if (!this->callback) return;

  // The callback lock is guaranteed initialized, because *locks->primary_mu
  // exists.
  absl::Mutex* callback_mu = &this->locks->callback_mu;

  // When executing the callback we need the primary flag's mutex to be unlocked
  // so that callback can retrieve the flag's value.
  this->locks->primary_mu.Unlock();

  {
    absl::MutexLock lock(callback_mu);
    this->callback();
  }

  this->locks->primary_mu.Lock();
}

// Attempts to parse supplied `value` string using parsing routine in the `flag`
// argument. If parsing is successful, it will try to validate that the parsed
// value is valid for the specified 'flag'. Finally this function stores the
// parsed value in 'dst' assuming it is a pointer to the flag's value type. In
// case if any error is encountered in either step, the error message is stored
// in 'err'
bool TryParseLocked(CommandLineFlag* flag, void* dst, absl::string_view value,
                    std::string* err)
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(flag->locks->primary_mu) {
  void* tentative_value = Clone(flag->op, flag->def);
  std::string parse_err;
  if (!Parse(flag->marshalling_op, value, tentative_value, &parse_err)) {
    auto type_name = flag->Typename();
    absl::string_view err_sep = parse_err.empty() ? "" : "; ";
    absl::string_view typename_sep = type_name.empty() ? "" : " ";
    *err = absl::StrCat("Illegal value '", value, "' specified for",
                        typename_sep, type_name, " flag '", flag->Name(), "'",
                        err_sep, parse_err);
    Delete(flag->op, tentative_value);
    return false;
  }

  if (!flag->InvokeValidator(tentative_value)) {
    *err = absl::StrCat("Failed validation of new value '",
                        Unparse(flag->marshalling_op, tentative_value),
                        "' for flag '", flag->Name(), "'");
    Delete(flag->op, tentative_value);
    return false;
  }

  flag->counter++;
  Copy(flag->op, tentative_value, dst);
  Delete(flag->op, tentative_value);
  return true;
}

// Sets the value of the flag based on specified string `value`. If the flag
// was successfully set to new value, it returns true. Otherwise, sets `err`
// to indicate the error, leaves the flag unchanged, and returns false. There
// are three ways to set the flag's value:
//  * Update the current flag value
//  * Update the flag's default value
//  * Update the current flag value if it was never set before
// The mode is selected based on 'set_mode' parameter.
bool CommandLineFlag::SetFromString(absl::string_view value,
                                    FlagSettingMode set_mode,
                                    ValueSource source, std::string* err) {
  if (IsRetired()) return false;

  absl::MutexLock l(InitFlagIfNecessary());

  // Direct-access flags can be modified without going through the
  // flag API. Detect such changes and update the flag->modified bit.
  if (!IsAbseilFlag()) {
    if (!this->modified && ChangedDirectly(this, this->cur, this->def)) {
      this->modified = true;
    }
  }

  switch (set_mode) {
    case SET_FLAGS_VALUE: {
      // set or modify the flag's value
      if (!TryParseLocked(this, this->cur, value, err)) return false;
      this->modified = true;
      UpdateCopy(this);
      InvokeCallback();

      if (source == kCommandLine) {
        this->on_command_line = true;
      }
      break;
    }
    case SET_FLAG_IF_DEFAULT: {
      // set the flag's value, but only if it hasn't been set by someone else
      if (!this->modified) {
        if (!TryParseLocked(this, this->cur, value, err)) return false;
        this->modified = true;
        UpdateCopy(this);
        InvokeCallback();
      } else {
        // TODO(rogeeff): review and fix this semantic. Currently we do not fail
        // in this case if flag is modified. This is misleading since the flag's
        // value is not updated even though we return true.
        // *err = absl::StrCat(this->Name(), " is already set to ",
        //                     CurrentValue(), "\n");
        // return false;
        return true;
      }
      break;
    }
    case SET_FLAGS_DEFAULT: {
      // modify the flag's default-value
      if (!TryParseLocked(this, this->def, value, err)) return false;

      if (!this->modified) {
        // Need to set both defvalue *and* current, in this case
        Copy(this->op, this->def, this->cur);
        UpdateCopy(this);
        InvokeCallback();
      }
      break;
    }
    default: {
      // unknown set_mode
      assert(false);
      return false;
    }
  }

  return true;
}

void CommandLineFlag::StoreAtomic(size_t size) {
  int64_t t = 0;
  assert(size <= sizeof(int64_t));
  memcpy(&t, this->cur, size);
  this->atomic.store(t, std::memory_order_release);
}

void CommandLineFlag::CheckDefaultValueParsingRoundtrip() const {
  std::string v = DefaultValue();

  absl::MutexLock lock(InitFlagIfNecessary());

  void* dst = Clone(this->op, this->def);
  std::string error;
  if (!flags_internal::Parse(this->marshalling_op, v, dst, &error)) {
    ABSL_INTERNAL_LOG(
        FATAL,
        absl::StrCat("Flag ", Name(), " (from ", Filename(),
                     "): std::string form of default value '", v,
                     "' could not be parsed; error=", error));
  }

  // We do not compare dst to def since parsing/unparsing may make
  // small changes, e.g., precision loss for floating point types.
  Delete(this->op, dst);
}

bool CommandLineFlag::ValidateDefaultValue() const {
  absl::MutexLock lock(InitFlagIfNecessary());
  return InvokeValidator(this->def);
}

bool CommandLineFlag::ValidateInputValue(absl::string_view value) const {
  absl::MutexLock l(InitFlagIfNecessary());  // protect default value access

  void* obj = Clone(this->op, this->def);
  std::string ignored_error;
  const bool result =
      flags_internal::Parse(this->marshalling_op, value, obj, &ignored_error) &&
      InvokeValidator(obj);
  Delete(this->op, obj);
  return result;
}

const int64_t CommandLineFlag::kAtomicInit;

void CommandLineFlag::Read(void* dst,
                           const flags_internal::FlagOpFn dst_op) const {
  absl::ReaderMutexLock l(InitFlagIfNecessary());

  // `dst_op` is the unmarshaling operation corresponding to the declaration
  // visibile at the call site. `op` is the Flag's defined unmarshalling
  // operation. They must match for this operation to be well-defined.
  if (ABSL_PREDICT_FALSE(dst_op != op)) {
    ABSL_INTERNAL_LOG(
        ERROR,
        absl::StrCat("Flag '", name,
                     "' is defined as one type and declared as another"));
  }
  CopyConstruct(op, cur, dst);
}

void CommandLineFlag::Write(const void* src,
                            const flags_internal::FlagOpFn src_op) {
  absl::MutexLock l(InitFlagIfNecessary());

  // `src_op` is the marshalling operation corresponding to the declaration
  // visible at the call site. `op` is the Flag's defined marshalling operation.
  // They must match for this operation to be well-defined.
  if (ABSL_PREDICT_FALSE(src_op != op)) {
    ABSL_INTERNAL_LOG(
        ERROR,
        absl::StrCat("Flag '", name,
                     "' is defined as one type and declared as another"));
  }

  if (ShouldValidateFlagValue(*this)) {
    void* obj = Clone(op, src);
    std::string ignored_error;
    std::string src_as_str = Unparse(marshalling_op, src);
    if (!Parse(marshalling_op, src_as_str, obj, &ignored_error) ||
        !InvokeValidator(obj)) {
      ABSL_INTERNAL_LOG(ERROR, absl::StrCat("Attempt to set flag '", name,
                                            "' to invalid value ", src_as_str));
    }
    Delete(op, obj);
  }

  modified = true;
  counter++;
  Copy(op, src, cur);

  UpdateCopy(this);
  InvokeCallback();
}

std::string HelpText::GetHelpText() const {
  if (help_function_) return help_function_();
  if (help_message_) return help_message_;

  return {};
}

// Update any copy of the flag value that is stored in an atomic word.
// In addition if flag has a mutation callback this function invokes it.
void UpdateCopy(CommandLineFlag* flag) {
#define STORE_ATOMIC(T)           \
  else if (flag->IsOfType<T>()) { \
    flag->StoreAtomic(sizeof(T)); \
  }

  if (false) {
  }
  ABSL_FLAGS_INTERNAL_FOR_EACH_LOCK_FREE(STORE_ATOMIC)
#undef STORE_ATOMIC
}

// Return true iff flag value was changed via direct-access.
bool ChangedDirectly(CommandLineFlag* flag, const void* a, const void* b) {
  if (!flag->IsAbseilFlag()) {
// Need to compare values for direct-access flags.
#define CHANGED_FOR_TYPE(T)                                                  \
  if (flag->IsOfType<T>()) {                                                 \
    return *reinterpret_cast<const T*>(a) != *reinterpret_cast<const T*>(b); \
  }

    CHANGED_FOR_TYPE(bool);
    CHANGED_FOR_TYPE(int32_t);
    CHANGED_FOR_TYPE(int64_t);
    CHANGED_FOR_TYPE(uint64_t);
    CHANGED_FOR_TYPE(double);
    CHANGED_FOR_TYPE(std::string);
#undef CHANGED_FOR_TYPE
  }

  return false;
}

}  // namespace flags_internal
}  // namespace absl
