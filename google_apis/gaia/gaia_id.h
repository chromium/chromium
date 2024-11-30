// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_ID_H_
#define GOOGLE_APIS_GAIA_GAIA_ID_H_

#include <iosfwd>
#include <string>
#include <string_view>

#include "base/component_export.h"

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

  GaiaId() = default;
  // Temporary implicit conversion to allow splitting code changes.
  // TODO(crbug.com/380416867): Make the constructor explicit.
  GaiaId(std::string value);
  GaiaId(const GaiaId&) = default;
  GaiaId(GaiaId&&) noexcept = default;
  ~GaiaId() = default;

  GaiaId& operator=(const GaiaId&) = default;
  GaiaId& operator=(GaiaId&&) noexcept = default;

  // Temporary implicit conversion to allow splitting code changes.
  // TODO(crbug.com/380416867): Remove implicit conversions.
  GaiaId(const char gaia_id[]) { id_ = gaia_id; }
  operator const std::string&() const { return id_; }
  operator std::string_view() const { return id_; }

  // Convenience functions to allow a more gradual adoption of this class where
  // std::string was used previously.
  // TODO(crbug.com/380416867): Remove convenience functions.
  const char* c_str() const { return id_.c_str(); }
  std::string::const_iterator begin() const { return id_.begin(); }
  std::string::const_iterator end() const { return id_.end(); }

  // Checks if the ID is valid or not.
  bool empty() const;

  const std::string& ToString() const;

  // Default comparisons.
  friend bool operator==(const GaiaId&, const GaiaId&) = default;
  friend auto operator<=>(const GaiaId&, const GaiaId&) = default;

 private:
  std::string id_;
};

COMPONENT_EXPORT(GOOGLE_APIS)
std::ostream& operator<<(std::ostream& out, const GaiaId& id);

#endif  // GOOGLE_APIS_GAIA_GAIA_ID_H_
