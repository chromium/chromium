// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_ID_H_
#define GOOGLE_APIS_GAIA_GAIA_ID_H_

#include <iosfwd>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS) && defined(__OBJC__)
@class NSString;
#endif  // BUILDFLAG(IS_IOS) && defined(__OBJC__)

// A string-like object representing an obfuscated Gaia ID that allows
// identifying a Google account. This value can be safely persisted to disk as
// it remains stable over time, but for additional privacy it is generally
// preferred to store it in hashed form when possible.
class COMPONENT_EXPORT(GOOGLE_APIS) GaiaId {
 public:
  struct Hash {
    size_t operator()(const GaiaId& gaia_id) const {
      return std::hash<std::string>()(gaia_id.ToString());
    }
  };

  // Constructs an empty instance.
  GaiaId() = default;
  // Explicit construction from std::string.
  explicit GaiaId(std::string value);
  // On iOS, allow direct construction from NSString, which is fairly common.
#if BUILDFLAG(IS_IOS) && defined(__OBJC__)
  explicit GaiaId(NSString* value);
#endif  // BUILDFLAG(IS_IOS) && defined(__OBJC__)
  GaiaId(const GaiaId&) = default;
  GaiaId(GaiaId&&) noexcept = default;
  ~GaiaId() = default;

  GaiaId& operator=(const GaiaId&) = default;
  GaiaId& operator=(GaiaId&&) noexcept = default;

  // Checks if the ID is valid or not.
  bool empty() const;

  [[nodiscard]] const std::string& ToString() const;

#if BUILDFLAG(IS_IOS) && defined(__OBJC__)
  [[nodiscard]] NSString* ToNSString() const;
#endif  // BUILDFLAG(IS_IOS) && defined(__OBJC__)

  // Default comparisons.
  friend bool operator==(const GaiaId&, const GaiaId&) = default;
  friend auto operator<=>(const GaiaId&, const GaiaId&) = default;

  // Convenience test-only class that allows defining constexpr or static
  // values and can be implicitly converted to GaiaId. Prefer using GaiaId
  // directly where possible, i.e. in all cases except those where the C++ style
  // guide disallows constructing a GaiaId instance (variables with static
  // storage duration, see
  // https://google.github.io/styleguide/cppguide.html#Static_and_Global_Variables
  // for more information).
#if defined(UNIT_TEST)
  class Literal {
   public:
    constexpr explicit Literal(std::string_view gaia_id) : gaia_id_(gaia_id) {}
    ~Literal() = default;

    // Allow implicit conversion to GaiaId.
    operator GaiaId() const { return GaiaId(std::string(gaia_id_)); }

    std::string ToString() const { return std::string(gaia_id_); }

   private:
    std::string_view gaia_id_;
  };
#endif  // defined(UNIT_TEST)

 private:
  std::string id_;
};

COMPONENT_EXPORT(GOOGLE_APIS)
std::ostream& operator<<(std::ostream& out, const GaiaId& id);

#endif  // GOOGLE_APIS_GAIA_GAIA_ID_H_
