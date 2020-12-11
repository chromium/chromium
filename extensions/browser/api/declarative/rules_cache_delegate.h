// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class RulesRegistry;

// RulesCacheDelegate implements the part of the RulesRegistry which works on
// the UI thread. It should only be used on the UI thread.
// If |log_storage_init_delay| is set, the delay caused by loading and
// registering rules on initialization will be logged with UMA.
class RulesCacheDelegate {
 public:
  class Observer {
   public:
    // Called when |UpdateRules| is called on the |RulesCacheDelegate|.
    virtual void OnUpdateRules() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Determines the type of a cache, indicating whether or not its rules are
  // persisted to storage.
  enum class Type {
    // An ephemeral RulesCacheDelegate never persists to storage when
    // |UpdateRules()| is called. It merely tracks rule state on the UI thread.
    kEphemeral,

    // Persistent RulesCacheDelegate writes the new rule set into storage every
    // time |UpdateRules()| is called, in addition to tracking rule state on the
    // UI thread.
    kPersistent,
  };

  RulesCacheDelegate(Type type, bool log_storage_init_delay);

  virtual ~RulesCacheDelegate();

  Type type() const { return type_; }

  // Returns a key for the state store. The associated preference is a boolean
  // indicating whether there are some declarative rules stored in the rule
  // store.
  static std::string GetRulesStoredKey(const std::string& event_name,
                                       bool incognito);

  // Initialize the storage functionality.
  void Init(RulesRegistry* registry);

  void UpdateRules(const std::string& extension_id, base::Value value);

  // Indicates whether or not this registry has any registered rules cached.
  bool HasRules() const;

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::WeakPtr<RulesCacheDelegate> GetWeakPtr() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(RulesRegistryWithCacheTest,
                           DeclarativeRulesStored);
  FRIEND_TEST_ALL_PREFIXES(RulesRegistryWithCacheTest,
                           RulesStoredFlagMultipleRegistries);

  const Type type_;

  static const char kRulesStoredKey[];

  // Check if we are done reading all data from storage on startup, and notify
  // the RulesRegistry on its thread if so. The notification is delivered
  // exactly once.
  void CheckIfReady();

  // Schedules retrieving rules for already loaded extensions where
  // appropriate.
  void ReadRulesForInstalledExtensions();

  // Read/write a list of rules serialized to Values.
  void ReadFromStorage(const std::string& extension_id);
  void ReadFromStorageCallback(const std::string& extension_id,
                               std::unique_ptr<base::Value> value);

  // Check the preferences whether the extension with |extension_id| has some
  // rules stored on disk. If this information is not in the preferences, true
  // is returned as a safe default value.
  bool GetDeclarativeRulesStored(const std::string& extension_id) const;
  // Modify the preference to |rules_stored|.
  void SetDeclarativeRulesStored(const std::string& extension_id,
                                 bool rules_stored);

  content::BrowserContext* browser_context_;

  // Indicates whether the ruleset is non-empty. Valid for both |kEphemeral| and
  // |kPersistent| cache types.
  bool has_nonempty_ruleset_ = false;

  // The key under which rules are stored. Only used for |kPersistent| caches.
  std::string storage_key_;

  // The key under which we store whether the rules have been stored. Only used
  // for |kPersistent| caches.
  std::string rules_stored_key_;

  // A set of extension IDs that have rules we are reading from storage.
  std::set<std::string> waiting_for_extensions_;

  // We measure the time spent on loading rules on init. The result is logged
  // with UMA once per each RulesCacheDelegate instance, unless in Incognito.
  base::Time storage_init_time_;
  bool log_storage_init_delay_;

  // Weak pointer to post tasks to the owning rules registry.
  base::WeakPtr<RulesRegistry> registry_;

  // The thread |registry_| lives on.
  content::BrowserThread::ID rules_registry_thread_;

  // We notified the RulesRegistry that the rules are loaded.
  bool notified_registry_;

  base::ObserverList<Observer>::Unchecked observers_;

  // Use this factory to generate weak pointers bound to the UI thread.
  base::WeakPtrFactory<RulesCacheDelegate> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_RULES_CACHE_DELEGATE_H__
