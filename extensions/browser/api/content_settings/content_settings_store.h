// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_STORE_H_
#define EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "extensions/common/api/types.h"
#include "extensions/common/extension_id.h"

class GURL;
class ContentSettingsPattern;

namespace content_settings {
class OriginValueMap;
class RuleIterator;
struct Rule;
}

namespace extensions {

// This class is the backend for extension-defined content settings. It is used
// by the content_settings::CustomExtensionProvider to integrate its settings
// into the HostContentSettingsMap and by the content settings extension API to
// provide extensions with access to content settings.
class ContentSettingsStore
    : public base::RefCountedThreadSafe<ContentSettingsStore> {
 public:
  using ChromeSettingScope = extensions::api::types::ChromeSettingScope;

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a content setting changes in the
    // ContentSettingsStore.
    virtual void OnContentSettingChanged(const ExtensionId& extension_id,
                                         bool incognito) = 0;
  };

  static constexpr char kContentSettingKey[] = "setting";
  static constexpr char kContentSettingsTypeKey[] = "type";
  static constexpr char kPrimaryPatternKey[] = "primaryPattern";
  static constexpr char kSecondaryPatternKey[] = "secondaryPattern";

  ContentSettingsStore();

  ContentSettingsStore(const ContentSettingsStore&) = delete;
  ContentSettingsStore& operator=(const ContentSettingsStore&) = delete;

  // //////////////////////////////////////////////////////////////////////////

  // See GetRuleIterator::GetRuleIterator().
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType type,
      bool incognito) const;

  // See GetRuleIterator::GetRule().
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record) const;

  // Sets the content |setting| for |pattern| of extension |ext_id|. The
  // |incognito| flag allow to set whether the provided setting is for
  // incognito mode only.
  // Precondition: the extension must be registered.
  // This method should only be called on the UI thread.
  // This method is called on startup to load from extension prefs. This method
  // is called each time an extension changes content settings.
  void SetExtensionContentSetting(
      const std::string& ext_id,
      const ContentSettingsPattern& embedded_pattern,
      const ContentSettingsPattern& top_level_pattern,
      ContentSettingsType type,
      ContentSetting setting,
      ChromeSettingScope scope);

  // Clears all contents settings set by the extension |ext_id|.
  void ClearContentSettingsForExtension(const std::string& ext_id,
                                        ChromeSettingScope scope);

  // Clears all contents settings set by the extension |ext_id| for the
  // content type |content_type|.
  void ClearContentSettingsForExtensionAndContentType(
      const std::string& ext_id,
      ChromeSettingScope scope,
      ContentSettingsType content_type);

  // Serializes all content settings set by the extension with ID |extension_id|
  // and returns them as a list of Values.
  base::Value::List GetSettingsForExtension(const ExtensionId& extension_id,
                                            ChromeSettingScope scope) const;

  // Deserializes content settings rules from |list| and applies them as set by
  // the extension with ID |extension_id|.
  void SetExtensionContentSettingFromList(const ExtensionId& extension_id,
                                          const base::Value::List& list,
                                          ChromeSettingScope scope);

  // //////////////////////////////////////////////////////////////////////////

  // Registers the time when an extension |ext_id| is installed.
  void RegisterExtension(const std::string& ext_id,
                         const base::Time& install_time,
                         bool is_enabled);

  // Deletes all entries related to extension |ext_id|.
  void UnregisterExtension(const std::string& ext_id);

  // Hides or makes the extension content settings of the specified extension
  // visible.
  void SetExtensionState(const std::string& ext_id, bool is_enabled);

  // Adds |observer|. This method should only be called on the UI thread.
  void AddObserver(Observer* observer);

  // Remove |observer|. This method should only be called on the UI thread.
  void RemoveObserver(Observer* observer);

 private:
  friend class base::RefCountedThreadSafe<ContentSettingsStore>;

  struct ExtensionEntry;

  // A list of the entries, maintained in reverse-chronological order (most-
  // recently installed items first) to facilitate search.
  using ExtensionEntries = std::vector<std::unique_ptr<ExtensionEntry>>;

  virtual ~ContentSettingsStore();

  content_settings::OriginValueMap* GetValueMap(const std::string& ext_id,
                                                ChromeSettingScope scope)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const content_settings::OriginValueMap* GetValueMap(
      const std::string& ext_id,
      ChromeSettingScope scope) const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void NotifyOfContentSettingChanged(const ExtensionId& extension_id,
                                     bool incognito);

  bool OnCorrectThread();

  ExtensionEntry* FindEntry(const std::string& ext_id) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ExtensionEntries::iterator FindIterator(const std::string& ext_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void ShouldNotifyForEntry(const ExtensionEntry& entry,
                            bool* notify,
                            bool* notify_incognito);

  // The entries.
  ExtensionEntries entries_ GUARDED_BY(lock_);

  base::ObserverList<Observer, false>::Unchecked observers_;

  mutable base::Lock lock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_STORE_H_
