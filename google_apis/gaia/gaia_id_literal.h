// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_ID_LITERAL_H_
#define GOOGLE_APIS_GAIA_GAIA_ID_LITERAL_H_

#include <string>
#include <string_view>

class GaiaId;

// Convenience test-only class that allows defining constexpr or static
// values and can be implicitly converted to GaiaId. Prefer using GaiaId
// directly where possible, i.e. in all cases except those where the C++ style
// guide disallows constructing a GaiaId instance (variables with static
// storage duration, see
// https://google.github.io/styleguide/cppguide.html#Static_and_Global_Variables
// for more information).
class GaiaIdLiteral {
 public:
  constexpr explicit GaiaIdLiteral(std::string_view gaia_id)
      : gaia_id_(gaia_id) {}
  ~GaiaIdLiteral() = default;

  // Allow implicit conversion to GaiaId.
  inline operator GaiaId() const { return GaiaId(std::string(gaia_id_)); }

  inline std::string ToString() const { return std::string(gaia_id_); }

 private:
  std::string_view gaia_id_;
};

#endif  // GOOGLE_APIS_GAIA_GAIA_ID_LITERAL_H_
