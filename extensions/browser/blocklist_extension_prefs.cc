// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blocklist_extension_prefs.h"

#include <optional>

#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

// If extension is blocklisted by Omaha attributes.
constexpr const char kPrefOmahaBlocklistState[] = "omaha_blocklist_state";

// If the user has acknowledged the blocklist state.
constexpr const char kPrefAcknowledgedBlocklistState[] =
    "acknowledged_blocklist_state";

// If extension is blocklisted or greylisted.
constexpr const char kPrefBlocklistState[] = "blacklist_state";

// If extension is blocklisted by the Extension Telemetry service.
constexpr const char kPrefExtensionTelemetryServiceBlocklistState[] =
    "extension_telemetry_service_blocklist_state";

// The default value to use for getting blocklist state from the pref.
constexpr BitMapBlocklistState kDefaultBitMapBlocklistState =
    BitMapBlocklistState::NOT_BLOCKLISTED;

// Extensions in these states should be put into the extension greylist.
// This list is sorted by the precedence order. When two states are presented
// at the same time, the state with higher precedence takes effect.
const BitMapBlocklistState kGreylistStates[] = {
    BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
    BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
    BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY};
const int kAllGreylistStates =
    static_cast<int>(BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY) |
    static_cast<int>(BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION) |
    static_cast<int>(BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED);

// Converts BitMapBlocklistState to BlocklistState.
BlocklistState BitMapBlocklistStateToBlocklistState(
    BitMapBlocklistState blocklist_state) {
  switch (blocklist_state) {
    case BitMapBlocklistState::NOT_BLOCKLISTED:
      return NOT_BLOCKLISTED;
    case BitMapBlocklistState::BLOCKLISTED_MALWARE:
      return BLOCKLISTED_MALWARE;
    case BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY:
      return BLOCKLISTED_SECURITY_VULNERABILITY;
    case BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION:
      return BLOCKLISTED_CWS_POLICY_VIOLATION;
    case BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED:
      return BLOCKLISTED_POTENTIALLY_UNWANTED;
  }
}

}  // namespace

namespace blocklist_prefs {

BitMapBlocklistState BlocklistStateToBitMapBlocklistState(
    BlocklistState blocklist_state) {
  switch (blocklist_state) {
    case NOT_BLOCKLISTED:
      return BitMapBlocklistState::NOT_BLOCKLISTED;
    case BLOCKLISTED_MALWARE:
      return BitMapBlocklistState::BLOCKLISTED_MALWARE;
    case BLOCKLISTED_SECURITY_VULNERABILITY:
      return BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY;
    case BLOCKLISTED_CWS_POLICY_VIOLATION:
      return BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION;
    case BLOCKLISTED_POTENTIALLY_UNWANTED:
      return BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED;
    case BLOCKLISTED_UNKNOWN:
      NOTREACHED_IN_MIGRATION()
          << "The unknown state should not be added into prefs.";
      return BitMapBlocklistState::NOT_BLOCKLISTED;
  }
}

BitMapBlocklistState GetExtensionBlocklistState(
    const ExtensionId& extension_id,
    const ExtensionPrefs* extension_prefs) {
  BitMapBlocklistState sb_state =
      GetSafeBrowsingExtensionBlocklistState(extension_id, extension_prefs);
  BitMapBlocklistState extension_telemetry_service_state =
      GetExtensionTelemetryServiceBlocklistState(extension_id, extension_prefs);
  if (sb_state == BitMapBlocklistState::BLOCKLISTED_MALWARE ||
      HasOmahaBlocklistState(extension_id,
                             BitMapBlocklistState::BLOCKLISTED_MALWARE,
                             extension_prefs) ||
      extension_telemetry_service_state ==
          BitMapBlocklistState::BLOCKLISTED_MALWARE) {
    return BitMapBlocklistState::BLOCKLISTED_MALWARE;
  }

  for (auto greylist_state : kGreylistStates) {
    if (sb_state == greylist_state ||
        HasOmahaBlocklistState(extension_id, greylist_state, extension_prefs)) {
      return greylist_state;
    }
  }

  return BitMapBlocklistState::NOT_BLOCKLISTED;
}

bool IsExtensionBlocklisted(const ExtensionId& extension_id,
                            ExtensionPrefs* extension_prefs) {
  return GetExtensionBlocklistState(extension_id, extension_prefs) ==
         BitMapBlocklistState::BLOCKLISTED_MALWARE;
}

void AddOmahaBlocklistState(const ExtensionId& extension_id,
                            BitMapBlocklistState state,
                            ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BitMapPrefOperation::kAdd, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

void RemoveOmahaBlocklistState(const ExtensionId& extension_id,
                               BitMapBlocklistState state,
                               ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BitMapPrefOperation::kRemove, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

bool HasOmahaBlocklistState(const ExtensionId& extension_id,
                            BitMapBlocklistState state,
                            const ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & static_cast<int>(state)) != 0;
}

bool HasAnyOmahaGreylistState(const ExtensionId& extension_id,
                              ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & kAllGreylistStates) != 0;
}

void AddAcknowledgedBlocklistState(const ExtensionId& extension_id,
                                   BitMapBlocklistState state,
                                   ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BitMapPrefOperation::kAdd,
      kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

void RemoveAcknowledgedBlocklistState(
    const ExtensionId& extension_id,
    BitMapBlocklistState state,
    extensions::ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BitMapPrefOperation::kRemove,
      kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

void ClearAcknowledgedGreylistStates(const ExtensionId& extension_id,
                                     ExtensionPrefs* extension_prefs) {
  for (auto state : kGreylistStates) {
    RemoveAcknowledgedBlocklistState(extension_id, state, extension_prefs);
  }
}

bool HasAcknowledgedBlocklistState(const ExtensionId& extension_id,
                                   BitMapBlocklistState state,
                                   const ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & static_cast<int>(state)) != 0;
}

void UpdateCurrentGreylistStatesAsAcknowledged(
    const ExtensionId& extension_id,
    ExtensionPrefs* extension_prefs) {
  for (auto state : kGreylistStates) {
    bool is_on_sb_list = (GetSafeBrowsingExtensionBlocklistState(
                              extension_id, extension_prefs) == state);
    bool is_on_omaha_list =
        HasOmahaBlocklistState(extension_id, state, extension_prefs);
    if (is_on_sb_list || is_on_omaha_list) {
      AddAcknowledgedBlocklistState(extension_id, state, extension_prefs);
    } else {
      RemoveAcknowledgedBlocklistState(extension_id, state, extension_prefs);
    }
  }
}

void SetSafeBrowsingExtensionBlocklistState(
    const ExtensionId& extension_id,
    BitMapBlocklistState bitmap_blocklist_state,
    ExtensionPrefs* extension_prefs) {
  if (bitmap_blocklist_state == BitMapBlocklistState::NOT_BLOCKLISTED) {
    extension_prefs->UpdateExtensionPref(extension_id, kPrefBlocklistState,
                                         std::nullopt);
    extension_prefs->DeleteExtensionPrefsIfPrefEmpty(extension_id);
  } else {
    extension_prefs->UpdateExtensionPref(
        extension_id, kPrefBlocklistState,
        base::Value(
            BitMapBlocklistStateToBlocklistState(bitmap_blocklist_state)));
  }
}

BitMapBlocklistState GetSafeBrowsingExtensionBlocklistState(
    const ExtensionId& extension_id,
    const ExtensionPrefs* extension_prefs) {
  int int_value = -1;
  if (extension_prefs->ReadPrefAsInteger(extension_id, kPrefBlocklistState,
                                         &int_value) &&
      int_value >= 0) {
    return BlocklistStateToBitMapBlocklistState(
        static_cast<BlocklistState>(int_value));
  }

  return BitMapBlocklistState::NOT_BLOCKLISTED;
}

void SetExtensionTelemetryServiceBlocklistState(
    const ExtensionId& extension_id,
    BitMapBlocklistState bitmap_blocklist_state,
    ExtensionPrefs* extension_prefs) {
  if (bitmap_blocklist_state == BitMapBlocklistState::NOT_BLOCKLISTED) {
    extension_prefs->UpdateExtensionPref(
        extension_id, kPrefExtensionTelemetryServiceBlocklistState,
        std::nullopt);
    extension_prefs->DeleteExtensionPrefsIfPrefEmpty(extension_id);
  } else {
    extension_prefs->UpdateExtensionPref(
        extension_id, kPrefExtensionTelemetryServiceBlocklistState,
        base::Value(
            BitMapBlocklistStateToBlocklistState(bitmap_blocklist_state)));
  }
}

BitMapBlocklistState GetExtensionTelemetryServiceBlocklistState(
    const ExtensionId& extension_id,
    const ExtensionPrefs* extension_prefs) {
  int int_value = -1;
  if (extension_prefs->ReadPrefAsInteger(
          extension_id, kPrefExtensionTelemetryServiceBlocklistState,
          &int_value) &&
      int_value >= 0) {
    return BlocklistStateToBitMapBlocklistState(
        static_cast<BlocklistState>(int_value));
  }

  return BitMapBlocklistState::NOT_BLOCKLISTED;
}

}  // namespace blocklist_prefs
}  // namespace extensions
