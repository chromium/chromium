// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_COMMAND_LINE_FLAGS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_COMMAND_LINE_FLAGS_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/export_template.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_export.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_flags.h"

namespace quiche {

// Abstract class for setting flags and fetching help strings.
class QuicheFlagHelper {
 public:
  explicit QuicheFlagHelper(const char* help) : help_(help) {}
  virtual ~QuicheFlagHelper() = default;

  virtual bool SetFlag(const std::string& value) const = 0;
  virtual void ResetFlag() const = 0;
  std::string GetHelp() const { return help_; }

 private:
  std::string help_;
};

// Templated class for setting flags of various types.
template <typename T>
class TypedQuicheFlagHelper : public QuicheFlagHelper {
 public:
  TypedQuicheFlagHelper(T* flag, const T& default_value, const char* help)
      : QuicheFlagHelper(help), flag_(flag), default_value_(default_value) {}
  bool SetFlag(const std::string& value) const override;
  void ResetFlag() const override { *flag_ = default_value_; }

 private:
  mutable raw_ptr<T> flag_;
  T default_value_;
};

// SetFlag specializations. Implementations in .cc file.
template <>
bool TypedQuicheFlagHelper<bool>::SetFlag(const std::string&) const;
template <>
bool TypedQuicheFlagHelper<uint16_t>::SetFlag(const std::string&) const;
template <>
bool TypedQuicheFlagHelper<int32_t>::SetFlag(const std::string&) const;
template <>
bool TypedQuicheFlagHelper<std::string>::SetFlag(const std::string&) const;

// TypedQuicheFlagHelper instantiations are in .cc file.
extern template class TypedQuicheFlagHelper<bool>;
extern template class TypedQuicheFlagHelper<uint16_t>;
extern template class TypedQuicheFlagHelper<int32_t>;
extern template class TypedQuicheFlagHelper<std::string>;

// Registry of QuicheFlagHelpers.
class QuicheFlagRegistry {
 public:
  ~QuicheFlagRegistry();

  static QuicheFlagRegistry& GetInstance();

  // Adds a flag to the registry.
  void RegisterFlag(const char* name, std::unique_ptr<QuicheFlagHelper> helper);

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
  friend class base::NoDestructor<QuicheFlagRegistry>;

  // Should only be accessed as a singleton.
  QuicheFlagRegistry();

  std::map<std::string, std::unique_ptr<QuicheFlagHelper>> flags_;
};

// Class instantiated in DEFINE_QUIC_COMMAND_LINE_FLAG_IMPL macro expansion,
// that registers flag as a side effect of its constructor. Similar in spirit to
// absl::flags_internal::FlagRegistrar.
template <typename T>
class QuicheFlagSetup {
 public:
  QuicheFlagSetup(T* flag,
                  const char* name,
                  const T& default_value,
                  const char* help) {
    QuicheFlagRegistry::GetInstance().RegisterFlag(
        name,
        std::make_unique<TypedQuicheFlagHelper<T>>(flag, default_value, help));
  }
  // Allow QuicheFlagSetup instance to convert to a bool in
  // DEFINE_QUIC_COMMAND_LINE_FLAG_IMPL macro expansion, so it can go away.
  operator bool() const { return true; }
};

// ------------------------------------------------------------------------
// DEFINE_QUICHE_COMMAND_LINE_FLAG implementation.
// ------------------------------------------------------------------------

#define DEFINE_QUICHE_COMMAND_LINE_FLAG_IMPL(type, name, default_value, help) \
  type FLAGS_##name = default_value;                                          \
  bool FLAGS_no##name =                                                       \
      quiche::QuicheFlagSetup<type>(&FLAGS_##name, #name, default_value, help)

std::vector<std::string> QuicheParseCommandLineFlagsImpl(
    const char* usage,
    int argc,
    const char* const* argv);

// Used internally by QuicheParseCommandLineFlagsImpl(), but exposed here for
// testing.
struct QuicheParseCommandLineFlagsResult {
  QuicheParseCommandLineFlagsResult();
  QuicheParseCommandLineFlagsResult(const QuicheParseCommandLineFlagsResult&);
  ~QuicheParseCommandLineFlagsResult();

  std::vector<std::string> non_flag_args;
  std::optional<int> exit_status;
};

QuicheParseCommandLineFlagsResult QuicheParseCommandLineFlagsHelper(
    const char* usage,
    const base::CommandLine& command_line);

void QuichePrintCommandLineFlagHelpImpl(const char* usage);

template <typename T>
T GetQuicheCommandLineFlag(const T& flag) {
  return flag;
}

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_COMMAND_LINE_FLAGS_IMPL_H_
