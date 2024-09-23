// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PREF_TYPES_H_
#define EXTENSIONS_BROWSER_PREF_TYPES_H_

namespace extensions {

enum PrefType {
  kBool,
  kString,
  kInteger,
  // TODO(archanasimha): implement Get/SetAsX for the following PrefTypes.
  kGURL,
  kList,
  kDictionary,
  //  kExtensionIdList,
  //  kPermissionSet,
  kTime
};

// PrefScope indicates whether an ExtensionPref is profile wide or specific to
// an extension. Extension-specific prefs are keyed under a dictionary with the
// extension ID and are removed when an extension is uninstalled.
enum class PrefScope { kProfile, kExtensionSpecific };

struct PrefMap {
  const char* name;
  PrefType type;
  PrefScope scope;
};

extern const PrefMap kCorruptedDisableCount;
extern const PrefMap kUserPermissions;
extern const PrefMap kUserScriptsWorldsConfiguration;
// TODO(crbug.com/337191307): Move pref to ManifestV2ExperimentManager and
// expose it as a public member.
extern const PrefMap kMV2DeprecationWarningAcknowledgedGloballyPref;
extern const PrefMap kMV2DeprecationDisabledAcknowledgedGloballyPref;
extern const PrefMap kMV2DeprecationUnsupportedAcknowledgedGloballyPref;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PREF_TYPES_H_
