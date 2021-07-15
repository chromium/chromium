// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blocklist_extension_prefs.h"

#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

// If extension is blocklisted by Omaha attributes.
constexpr const char kPrefOmahaBlocklistState[] = "omaha_blocklist_state";

// If the user has acknowledged the blocklist state.
constexpr const char kPrefAcknowledgedBlocklistState[] =
    "acknowledged_blocklist_state";

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
      NOTREACHED() << "The unknown state should not be added into prefs.";
      return BitMapBlocklistState::NOT_BLOCKLISTED;
  }
}

BitMapBlocklistState GetExtensionBlocklistState(
    const std::string& extension_id,
    ExtensionPrefs* extension_prefs) {
  BitMapBlocklistState sb_state = BlocklistStateToBitMapBlocklistState(
      extension_prefs->GetExtensionBlocklistState(extension_id));
  if (sb_state == BitMapBlocklistState::BLOCKLISTED_MALWARE ||
      HasOmahaBlocklistState(extension_id,
                             BitMapBlocklistState::BLOCKLISTED_MALWARE,
                             extension_prefs)) {
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

void AddOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state), ExtensionPrefs::BIT_MAP_PREF_ADD,
      kPrefOmahaBlocklistState, static_cast<int>(kDefaultBitMapBlocklistState));
}

void RemoveOmahaBlocklistState(const std::string& extension_id,
                               BitMapBlocklistState state,
                               ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BIT_MAP_PREF_REMOVE, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

bool HasOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & static_cast<int>(state)) != 0;
}

bool HasAnyOmahaGreylistState(const std::string& extension_id,
                              ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefOmahaBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & kAllGreylistStates) != 0;
}

void AddAcknowledgedBlocklistState(const std::string& extension_id,
                                   BitMapBlocklistState state,
                                   ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state), ExtensionPrefs::BIT_MAP_PREF_ADD,
      kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

void RemoveAcknowledgedBlocklistState(
    const std::string& extension_id,
    BitMapBlocklistState state,
    extensions::ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, static_cast<int>(state),
      ExtensionPrefs::BIT_MAP_PREF_REMOVE, kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

void ClearAcknowledgedBlocklistStates(const std::string& extension_id,
                                      ExtensionPrefs* extension_prefs) {
  extension_prefs->ModifyBitMapPrefBits(
      extension_id, 0, ExtensionPrefs::BIT_MAP_PREF_CLEAR,
      kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
}

bool HasAcknowledgedBlocklistState(const std::string& extension_id,
                                   BitMapBlocklistState state,
                                   ExtensionPrefs* extension_prefs) {
  int current_states = extension_prefs->GetBitMapPrefBits(
      extension_id, kPrefAcknowledgedBlocklistState,
      static_cast<int>(kDefaultBitMapBlocklistState));
  return (current_states & static_cast<int>(state)) != 0;
}

void UpdateCurrentGreylistStatesAsAcknowledged(
    const std::string& extension_id,
    ExtensionPrefs* extension_prefs) {
  for (auto state : kGreylistStates) {
    bool is_on_sb_list =
        (BlocklistStateToBitMapBlocklistState(
             extension_prefs->GetExtensionBlocklistState(extension_id)) ==
         state);
    bool is_on_omaha_list =
        HasOmahaBlocklistState(extension_id, state, extension_prefs);
    if (is_on_sb_list || is_on_omaha_list) {
      AddAcknowledgedBlocklistState(extension_id, state, extension_prefs);
    } else {
      RemoveAcknowledgedBlocklistState(extension_id, state, extension_prefs);
    }
  }
}

}  // namespace blocklist_prefs
}  // namespace extensions
