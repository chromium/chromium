// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_H_
#define EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/api/types.h"
#include "extensions/common/extension_id.h"

class PrefValueMap;

namespace base {
class Time;
class Value;
}

// Non-persistent data container that is shared by ExtensionPrefStores. All
// extension pref values (incognito and regular) are stored herein and
// provided to ExtensionPrefStores.
//
// The semantics of the ExtensionPrefValueMap are:
// - A regular setting applies to regular browsing sessions as well as incognito
//   browsing sessions.
// - An incognito setting applies only to incognito browsing sessions, not to
//   regular ones. It takes precedence over a regular setting set by the same
//   extension.
// - A regular-only setting applies only to regular browsing sessions, not to
//   incognito ones. It takes precedence over a regular setting set by the same
//   extension.
// - If two different extensions set a value for the same preference (and both
//   values apply to the regular/incognito browsing session), the extension that
//   was installed later takes precedence, regardless of whether the settings
//   are regular, incognito or regular-only.
//
// The following table illustrates the behavior:
//   A.reg | A.reg_only | A.inc | B.reg | B.reg_only | B.inc | E.reg | E.inc
//     1   |      -     |   -   |   -   |      -     |   -   |   1   |   1
//     1   |      2     |   -   |   -   |      -     |   -   |   2   |   1
//     1   |      -     |   3   |   -   |      -     |   -   |   1   |   3
//     1   |      2     |   3   |   -   |      -     |   -   |   2   |   3
//     1   |      -     |   -   |   4   |      -     |   -   |   4   |   4
//     1   |      2     |   3   |   4   |      -     |   -   |   4   |   4
//     1   |      -     |   -   |   -   |      5     |   -   |   5   |   1
//     1   |      -     |   3   |   4   |      5     |   -   |   5   |   4
//     1   |      -     |   -   |   -   |      -     |   6   |   1   |   6
//     1   |      2     |   -   |   4   |      -     |   6   |   4   |   6
//     1   |      2     |   3   |   -   |      5     |   6   |   5   |   6
//
// A = extension A, B = extension B, E = effective value
// .reg = regular value
// .reg_only = regular-only value
// .inc = incognito value
// Extension B has higher precedence than A.
class ExtensionPrefValueMap : public KeyedService {
 public:
  using ChromeSettingScope = extensions::api::types::ChromeSettingScope;

  // Observer interface for monitoring ExtensionPrefValueMap.
  class Observer {
   public:
    // Called when the value for the given |key| set by one of the extensions
    // changes. This does not necessarily mean that the effective value has
    // changed.
    virtual void OnPrefValueChanged(const std::string& key) = 0;
    // Notification about the ExtensionPrefValueMap being fully initialized.
    virtual void OnInitializationCompleted() = 0;
    // Called when the ExtensionPrefValueMap is being destroyed. When called,
    // observers must unsubscribe.
    virtual void OnExtensionPrefValueMapDestruction() = 0;

   protected:
    virtual ~Observer() {}
  };

  ExtensionPrefValueMap();

  ExtensionPrefValueMap(const ExtensionPrefValueMap&) = delete;
  ExtensionPrefValueMap& operator=(const ExtensionPrefValueMap&) = delete;

  ~ExtensionPrefValueMap() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Set an extension preference |value| for |key| of extension |ext_id|.
  // Note that regular extension pref values need to be reported to
  // incognito and to regular ExtensionPrefStores.
  // Precondition: the extension must be registered.
  void SetExtensionPref(const std::string& ext_id,
                        const std::string& key,
                        ChromeSettingScope scope,
                        base::Value value);

  // Remove the extension preference value for |key| of extension |ext_id|.
  // Precondition: the extension must be registered.
  void RemoveExtensionPref(const std::string& ext_id,
                           const std::string& key,
                           ChromeSettingScope scope);

  // Returns true if currently no extension with higher precedence controls the
  // preference. If |incognito| is true and the extension does not have
  // incognito permission, CanExtensionControlPref returns false.
  // Note that this function does does not consider the existence of
  // policies. An extension is only really able to control a preference if
  // PrefService::Preference::IsExtensionModifiable() returns true as well.
  bool CanExtensionControlPref(const extensions::ExtensionId& extension_id,
                               const std::string& pref_key,
                               bool incognito) const;

  // Removes all "incognito session only" preference values.
  void ClearAllIncognitoSessionOnlyPreferences();

  // Returns true if an extension identified by |extension_id| controls the
  // preference. This means this extension has set a preference value and no
  // other extension with higher precedence overrides it. If |from_incognito|
  // is not NULL, looks at incognito preferences first, and |from_incognito| is
  // set to true if the effective pref value is coming from the incognito
  // preferences, false if it is coming from the normal ones.
  // Note that the this function does does not consider the existence of
  // policies. An extension is only really able to control a preference if
  // PrefService::Preference::IsExtensionModifiable() returns true as well.
  bool DoesExtensionControlPref(const extensions::ExtensionId& extension_id,
                                const std::string& pref_key,
                                bool* from_incognito) const;

  // Returns the ID of the extension that currently controls this preference
  // for a regular profile. Incognito settings are ignored.
  // Returns an empty string if this preference is not controlled by an
  // extension.
  std::string GetExtensionControllingPref(const std::string& pref_key) const;

  // Tell the store it's now fully initialized.
  void NotifyInitializationCompleted();

  // Registers the time when an extension |ext_id| is installed.
  void RegisterExtension(const std::string& ext_id,
                         const base::Time& install_time,
                         bool is_enabled,
                         bool is_incognito_enabled);

  // Deletes all entries related to extension |ext_id|.
  void UnregisterExtension(const std::string& ext_id);

  // Hides or makes the extension preference values of the specified extension
  // visible.
  void SetExtensionState(const std::string& ext_id, bool is_enabled);

  // Sets whether the extension has permission to access incognito state.
  void SetExtensionIncognitoState(const std::string& ext_id,
                                  bool is_incognito_enabled);

  // Adds an observer and notifies it about the currently stored keys.
  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

  const base::Value* GetEffectivePrefValue(const std::string& key,
                                           bool incognito,
                                           bool* from_incognito) const;

 private:
  struct ExtensionEntry;

  typedef std::map<std::string, std::unique_ptr<ExtensionEntry>>
      ExtensionEntryMap;

  const PrefValueMap* GetExtensionPrefValueMap(const std::string& ext_id,
                                               ChromeSettingScope scope) const;

  PrefValueMap* GetExtensionPrefValueMap(const std::string& ext_id,
                                         ChromeSettingScope scope);

  // Returns all keys of pref values that are set by the extension of |entry|,
  // regardless whether they are set for incognito or regular pref values.
  void GetExtensionControlledKeys(const ExtensionEntry& entry,
                                  std::set<std::string>* out) const;

  // Returns an iterator to the extension which controls the preference |key|.
  // If |incognito| is true, looks at incognito preferences first. In that case,
  // if |from_incognito| is not NULL, it is set to true if the effective pref
  // value is coming from the incognito preferences, false if it is coming from
  // the normal ones.
  ExtensionEntryMap::const_iterator GetEffectivePrefValueController(
      const std::string& key,
      bool incognito,
      bool* from_incognito) const;

  void NotifyOfDestruction();
  void NotifyPrefValueChanged(const std::string& key);
  void NotifyPrefValueChanged(const std::set<std::string>& keys);

  // Mapping of which extension set which preference value. The effective
  // preferences values (i.e. the ones with the highest precedence)
  // are stored in ExtensionPrefStores.
  ExtensionEntryMap entries_;

  // In normal Profile shutdown, Shutdown() notifies observers that we are
  // being destroyed. In tests, it isn't called, so the notification must
  // be done in the destructor. This bit tracks whether it has been done yet.
  bool destroyed_;

  base::ObserverList<Observer, true>::Unchecked observers_;
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_PREF_VALUE_MAP_H_
