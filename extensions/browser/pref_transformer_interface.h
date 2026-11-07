// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PREF_TRANSFORMER_INTERFACE_H_
#define EXTENSIONS_BROWSER_PREF_TRANSFORMER_INTERFACE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class PrefModifiabilityObserver;

class PrefTransformerInterface {
 public:
  // Notified when `IsExtensionModifiable()` changes for a preference in a
  // `BrowserContext`.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPrefModifiabilityChanged(
        const std::string& browser_pref) = 0;
  };

  PrefTransformerInterface();
  virtual ~PrefTransformerInterface();

  // Converts the representation of a preference as seen by the extension
  // into a representation that is used in the pref stores of the browser.
  // Returns the pref store representation in case of success or sets
  // `error` and returns std::nullopt otherwise. `bad_message` is passed
  // to simulate the behavior of EXTENSION_FUNCTION_VALIDATE.
  virtual std::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) = 0;

  // Converts the representation of the preference as stored in the browser
  // into a representation that is used by the extension.
  // Returns the extension representation in case of success or returns
  // std::nullopt otherwise.
  virtual std::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) = 0;

  // Returns whether the preference can be modified by extensions in `context`.
  // `context` must be non-null and is not retained.
  virtual bool IsExtensionModifiable(content::BrowserContext* context) const;

  // Lazily starts monitoring modifiability for `context` when its first
  // observer is added, and stops when its last observer is removed. A
  // transformer that creates a `PrefModifiabilityObserver` must be 1:1 with a
  // `browser_pref`.
  void AddObserver(content::BrowserContext* context,
                   const std::string& browser_pref,
                   Observer* observer);
  void RemoveObserver(content::BrowserContext* context, Observer* observer);

 protected:
  virtual std::unique_ptr<PrefModifiabilityObserver>
  CreateModifiabilityObserver(content::BrowserContext* context);

 private:
  friend class PrefModifiabilityObserver;

  void OnModifiabilityChanged(content::BrowserContext* context,
                              const std::string& browser_pref);

  size_t notification_depth_ = 0;
  std::map<raw_ptr<content::BrowserContext>, base::ObserverList<Observer>>
      observers_;
  std::map<raw_ptr<content::BrowserContext>,
           std::unique_ptr<PrefModifiabilityObserver>>
      modifiability_observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PREF_TRANSFORMER_INTERFACE_H_
