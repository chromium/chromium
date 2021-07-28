// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BLOCKLIST_EXTENSION_PREFS_H_
#define EXTENSIONS_BROWSER_BLOCKLIST_EXTENSION_PREFS_H_

#include <string>

#include "extensions/browser/blocklist_state.h"

namespace extensions {
class ExtensionPrefs;

// Helper namespace for adding/removing/querying prefs for the blocklist.
namespace blocklist_prefs {

// Converts BlocklistState to BitMapBlocklistState.
BitMapBlocklistState BlocklistStateToBitMapBlocklistState(
    BlocklistState blocklist_state);

// Takes both Safe Browsing blocklist state and Omaha attribute blocklist states
// into account and determine the final state of the extension. The precedence
// is defined as follow:
// BLOCKLISTED_MALWARE > BLOCKLISTED_CWS_POLICY_VIOLATION >
// BLOCKLISTED_POTENTIALLY_UNWANTED > BLOCKLISTED_SECURITY_VULNERABILITY.
// TODO(crbug.com/1193695): Replace IsExtensionBlocklisted by this method.
BitMapBlocklistState GetExtensionBlocklistState(
    const std::string& extension_id,
    ExtensionPrefs* extension_prefs);

// Adds the `state` to the Omaha blocklist state pref.
void AddOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            ExtensionPrefs* extension_prefs);
// Removes the `state` from the Omaha blocklist state pref. It doesn't clear
// the other states in the pref.
void RemoveOmahaBlocklistState(const std::string& extension_id,
                               BitMapBlocklistState state,
                               ExtensionPrefs* extension_prefs);
// Checks whether the `extension_id` has the `state` in the Omaha blocklist
// state pref.
bool HasOmahaBlocklistState(const std::string& extension_id,
                            BitMapBlocklistState state,
                            ExtensionPrefs* extension_prefs);
// Checks whether the `extension_id` is in any Omaha greylist state.
bool HasAnyOmahaGreylistState(const std::string& extension_id,
                              ExtensionPrefs* extension_prefs);

// Adds the `state` to the acknowledged blocklist state pref.
void AddAcknowledgedBlocklistState(const std::string& extension_id,
                                   BitMapBlocklistState state,
                                   ExtensionPrefs* extension_prefs);
// Removes the `state` from the acknowledged blocklist state pref. It doesn't
// clear the other states in the pref.
void RemoveAcknowledgedBlocklistState(
    const std::string& extension_id,
    BitMapBlocklistState state,
    extensions::ExtensionPrefs* extension_prefs);
// Clears all states in the acknowledged blocklist state pref.
void ClearAcknowledgedBlocklistStates(const std::string& extension_id,
                                      ExtensionPrefs* extension_prefs);
// Checks whether the `extension_id` has the `state` in the acknowledged
// blocklist state pref.
bool HasAcknowledgedBlocklistState(const std::string& extension_id,
                                   BitMapBlocklistState state,
                                   ExtensionPrefs* extension_prefs);
// Sets all current greylist states for this `extension_id` as acknowledged.
// It will consider both Safe Browsing greylist state and Omaha attribute
// greylist state. Previous acknowledged states will be cleared if the
// `extension_id` is no longer in that state.
void UpdateCurrentGreylistStatesAsAcknowledged(const std::string& extension_id,
                                               ExtensionPrefs* extension_prefs);

// Sets the `bitmap_blocklist_state` to the Safe Browsing blocklist state pref.
void SetSafeBrowsingExtensionBlocklistState(
    const std::string& extension_id,
    BitMapBlocklistState bitmap_blocklist_state,
    ExtensionPrefs* extension_prefs);

// Returns the current Safe Browsing blocklist state of the `extension_id`.
// Warning: This function only takes Safe Browsing blocklist states into
// account. If you'd like to combine both Safe Browsing and Omaha attribute
// blocklist, please use blocklist_prefs::GetExtensionBlocklistState instead.
BitMapBlocklistState GetSafeBrowsingExtensionBlocklistState(
    const std::string& extension_id,
    ExtensionPrefs* extension_prefs);

// Returns whether the extension with |extension_id| has its blocklist bit set.
//
// WARNING: this only checks the extension's entry in prefs, so by definition
// can only check extensions that prefs knows about. There may be other
// sources of blocklist information, such as safebrowsing. You probably want
// to use GetExtensionBlocklistState rather than this method.
// TODO(crbug.com/1232243): Call GetExtensionBlocklistState to take both Safe
// Browsing and Omaha attribute blocklist states into account, since most
// callers don't need to differentiate the source of the blocklist state.
bool IsExtensionBlocklisted(const std::string& extension_id,
                            ExtensionPrefs* extension_prefs);

}  // namespace blocklist_prefs
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BLOCKLIST_EXTENSION_PREFS_H_
