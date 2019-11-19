// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/export_template.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

#define QUIC_FLAG(type, flag, value) QUIC_EXPORT_PRIVATE extern type flag;
#include "net/quic/quic_flags_list.h"
#undef QUIC_FLAG

// API compatibility with new-style flags.

inline bool GetQuicFlagImpl(bool flag) {
  return flag;
}
inline int32_t GetQuicFlagImpl(int32_t flag) {
  return flag;
}
inline uint32_t GetQuicFlagImpl(uint32_t flag) {
  return flag;
}
inline int64_t GetQuicFlagImpl(int64_t flag) {
  return flag;
}
inline uint64_t GetQuicFlagImpl(uint64_t flag) {
  return flag;
}
inline double GetQuicFlagImpl(double flag) {
  return flag;
}
inline std::string GetQuicFlagImpl(const std::string& flag) {
  return flag;
}

#define SetQuicFlagImpl(flag, value) ((flag) = (value))

// Sets the flag named |flag_name| to the value of |value| after converting
// it from a string to the appropriate type. If |value| is invalid or out of
// range, the flag will be unchanged.
QUIC_EXPORT_PRIVATE void SetQuicFlagByName(const std::string& flag_name,
                                           const std::string& value);

namespace quic {

// ------------------------------------------------------------------------
// DEFINE_QUIC_COMMAND_LINE_FLAG implementation.
// ------------------------------------------------------------------------

// Abstract class for setting flags and fetching help strings.
class QuicFlagHelper {
 public:
  explicit QuicFlagHelper(const char* help) : help_(help) {}
  virtual ~QuicFlagHelper() = default;

  virtual bool SetFlag(const std::string& value) const = 0;
  virtual void ResetFlag() const = 0;
  std::string GetHelp() const { return help_; }

 private:
  std::string help_;
};

// Templated class for setting flags of various types.
template <typename T>
class TypedQuicFlagHelper : public QuicFlagHelper {
 public:
  TypedQuicFlagHelper(T* flag, const T& default_value, const char* help)
      : QuicFlagHelper(help), flag_(flag), default_value_(default_value) {}
  bool SetFlag(const std::string& value) const override;
  void ResetFlag() const override { *flag_ = default_value_; }

 private:
  mutable T* flag_;
  T default_value_;
};

// SetFlag specializations. Implementations in .cc file.
template <>
QUIC_EXPORT_PRIVATE bool TypedQuicFlagHelper<bool>::SetFlag(
    const std::string&) const;
template <>
QUIC_EXPORT_PRIVATE bool TypedQuicFlagHelper<int32_t>::SetFlag(
    const std::string&) const;
template <>
QUIC_EXPORT_PRIVATE bool TypedQuicFlagHelper<std::string>::SetFlag(
    const std::string&) const;

// TypedQuicFlagHelper instantiations are in .cc file.
extern template class EXPORT_TEMPLATE_DECLARE(QUIC_EXPORT_PRIVATE)
    TypedQuicFlagHelper<bool>;
extern template class EXPORT_TEMPLATE_DECLARE(QUIC_EXPORT_PRIVATE)
    TypedQuicFlagHelper<int32_t>;
extern template class EXPORT_TEMPLATE_DECLARE(QUIC_EXPORT_PRIVATE)
    TypedQuicFlagHelper<std::string>;

// Registry of QuicFlagHelpers.
class QUIC_EXPORT_PRIVATE QuicFlagRegistry {
 public:
  ~QuicFlagRegistry();

  static QuicFlagRegistry& GetInstance();

  // Adds a flag to the registry.
  void RegisterFlag(const char* name, std::unique_ptr<QuicFlagHelper> helper);

  // Sets any flags in the registry that are specified in |command_line|,
  // returning true iff successful. If there is a failure, e.g. due to an
  // invalid flag value, returns false and sets |error_msg|.
  bool SetFlags(const base::CommandLine& command_line,
                std::string* error_msg) const;

  // Resets flags to their default values.
  void ResetFlags() const;

  // Returns a help string consisting of the names and help strings of all
  // registered flags.
  std::string GetHelp() const;

 private:
  friend class base::NoDestructor<QuicFlagRegistry>;

  // Should only be accessed as a singleton.
  QuicFlagRegistry();

  std::map<std::string, std::unique_ptr<QuicFlagHelper>> flags_;
};

// Class instantiated in DEFINE_QUIC_COMMAND_LINE_FLAG_IMPL macro expansion,
// that registers flag as a side effect of its constructor. Similar in spirit to
// absl::flags_internal::FlagRegistrar.
template <typename T>
class QuicFlagSetup {
 public:
  QuicFlagSetup(T* flag,
                const char* name,
                const T& default_value,
                const char* help) {
    QuicFlagRegistry::GetInstance().RegisterFlag(
        name,
        std::make_unique<TypedQuicFlagHelper<T>>(flag, default_value, help));
  }
  // Allow QuicFlagSetup instance to convert to a bool in
  // DEFINE_QUIC_COMMAND_LINE_FLAG_IMPL macro expansion, so it can go away.
  operator bool() const { return true; }
};

#define DEFINE_QUIC_COMMAND_LINE_FLAG_IMPL(type, name, default_value, help) \
  type FLAGS_##name = default_value;                                        \
  bool FLAGS_no##name =                                                     \
      quic::QuicFlagSetup<type>(&FLAGS_##name, #name, default_value, help)

QUIC_EXPORT_PRIVATE std::vector<std::string> QuicParseCommandLineFlagsImpl(
    const char* usage,
    int argc,
    const char* const* argv);

// Used internally by QuicParseCommandLineFlagsImpl(), but exposed here for
// testing.
struct QUIC_EXPORT_PRIVATE QuicParseCommandLineFlagsResult {
  QuicParseCommandLineFlagsResult();
  QuicParseCommandLineFlagsResult(const QuicParseCommandLineFlagsResult&);
  ~QuicParseCommandLineFlagsResult();

  std::vector<std::string> non_flag_args;
  base::Optional<int> exit_status;
};

QUIC_EXPORT_PRIVATE QuicParseCommandLineFlagsResult
QuicParseCommandLineFlagsHelper(const char* usage,
                                const base::CommandLine& command_line);

QUIC_EXPORT_PRIVATE void QuicPrintCommandLineFlagHelpImpl(const char* usage);

// ------------------------------------------------------------------------
// QUIC feature flags implementation.
// ------------------------------------------------------------------------
#define RELOADABLE_FLAG(flag) FLAGS_quic_reloadable_flag_##flag
#define RESTART_FLAG(flag) FLAGS_quic_restart_flag_##flag

#define GetQuicReloadableFlagImpl(flag) GetQuicFlag(RELOADABLE_FLAG(flag))
#define SetQuicReloadableFlagImpl(flag, value) \
  SetQuicFlag(RELOADABLE_FLAG(flag), value)
#define GetQuicRestartFlagImpl(flag) GetQuicFlag(RESTART_FLAG(flag))
#define SetQuicRestartFlagImpl(flag, value) \
  SetQuicFlag(RESTART_FLAG(flag), value)

}  // namespace quic
#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
