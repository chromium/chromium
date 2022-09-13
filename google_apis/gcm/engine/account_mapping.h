// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_ACCOUNT_MAPPING_H_
#define GOOGLE_APIS_GCM_ENGINE_ACCOUNT_MAPPING_H_

#include <string>

#include "base/time/time.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace gcm {

// Stores information about Account mapping and a last message sent regarding
// that mapping.
struct GCM_EXPORT AccountMapping {
  // Status of the account mapping.
  enum MappingStatus {
    NEW,       // This is a new account mapping entry.
    ADDING,    // A mapping message has been sent, but it has not been confirmed
               // yet.
    MAPPED,    // Account is mapped. At least one message has been confirmed to
               // reached the GCM.
    REMOVING,  // Account is removed, but a message removing the mapping has not
               // been confirmed yet.
  };

  AccountMapping();
  AccountMapping(const AccountMapping& other);
  ~AccountMapping();

  // Serializes account mapping to string without |account_id|, |status| or
  // |access_token|.
  std::string SerializeAsString() const;
  // Parses account mapping from store, without |account_id| or |access_token|.
  // |status| is infered.
  bool ParseFromString(const std::string& value);

  // Account Id of the account. (Acts as key for persistence.)
  CoreAccountId account_id;
  // Email address of the tracked account.
  std::string email;
  // OAuth2 access token used to authenticate mappings (not persisted).
  std::string access_token;
  // Status of the account mapping (not persisted).
  MappingStatus status;
  // Time of the mapping status change.
  base::Time status_change_timestamp;
  // ID of the last mapping message sent to GCM.
  std::string last_message_id;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_ACCOUNT_MAPPING_H_
