// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
#define GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

// Represent the id of an account for interaction with GAIA.
//
// --------------------------------------------------------------------------
// DO NOT USE CoreAccountId AS A PERSISTENT IDENTIFIER OF AN ACCOUNT.
//
// Currently a CoreAccountId can be created from a Gaia ID or from an email
// that was canonicalized. We are in the process of migrating this identifier
// to always be created from a Gaia ID.
// Until the migration is complete, the value of a CoreAccountId value may
// change on start-up.
// --------------------------------------------------------------------------

struct COMPONENT_EXPORT(GOOGLE_APIS) CoreAccountId {
  CoreAccountId();
  CoreAccountId(const CoreAccountId&);
  CoreAccountId(CoreAccountId&&) noexcept;
  ~CoreAccountId();

  CoreAccountId& operator=(const CoreAccountId&);
  CoreAccountId& operator=(CoreAccountId&&) noexcept;

  // Checks if the account is valid or not.
  bool empty() const;

  // Returns true if this CoreAccountId was created from an email.
  // Returns false if it is empty.
  bool IsEmail() const;

  // Return the string representation of a CoreAccountID.
  //
  // As explained above, the string representation of a CoreAccountId is
  // (for now) unstable and cannot be used to store serialized data to
  // persistent storage. Only in-memory storage is safe.
  const std::string& ToString() const;

  // -------------------------------------------------------------------------
  // --------------------------- DO NOT USE ----------------------------------
  // TL;DR: To get a CoreAccountId, please use the IdentityManager.
  //
  // All constructors of this class are private or only used for tests as
  // clients should not be creating CoreAccountId objects directly.

  // Create a CoreAccountId from a Gaia ID.
  // Returns an empty CoreAccountId if |gaia_id| is empty.
  static CoreAccountId FromGaiaId(const std::string& gaia_id);

  // Create a CoreAccountId object from an email of a robot account.
  // Returns an empty CoreAccountId if |email| is empty.
  static CoreAccountId FromRobotEmail(const std::string& robot_email);

  // Create a CoreAccountId object from a string that was serialized via
  // |CoreAccountId::ToString()|.
  static CoreAccountId FromString(const std::string& value);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only on ChromeOS, CoreAccountId objects may be created from Gaia emails.
  //
  // Create a CoreAccountId object from an email.
  // Returns an empty CoreAccountId if |email| is empty.
  static CoreAccountId FromEmail(const std::string& email);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  // ---------------------------------------- ---------------------------------

 private:
  std::string id_;
};

COMPONENT_EXPORT(GOOGLE_APIS)
bool operator<(const CoreAccountId& lhs, const CoreAccountId& rhs);

COMPONENT_EXPORT(GOOGLE_APIS)
bool operator==(const CoreAccountId& lhs, const CoreAccountId& rhs);

COMPONENT_EXPORT(GOOGLE_APIS)
bool operator!=(const CoreAccountId& lhs, const CoreAccountId& rhs);

COMPONENT_EXPORT(GOOGLE_APIS)
std::ostream& operator<<(std::ostream& out, const CoreAccountId& a);

// Returns the values of the account ids in a vector. Useful especially for
// logs.
COMPONENT_EXPORT(GOOGLE_APIS)
std::vector<std::string> ToStringList(
    const std::vector<CoreAccountId>& account_ids);

namespace std {
template <>
struct hash<CoreAccountId> {
  size_t operator()(const CoreAccountId& account_id) const {
    return std::hash<std::string>()(account_id.ToString());
  }
};
}  // namespace std

#endif  // GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
