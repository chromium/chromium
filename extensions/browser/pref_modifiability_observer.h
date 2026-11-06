// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PREF_MODIFIABILITY_OBSERVER_H_
#define EXTENSIONS_BROWSER_PREF_MODIFIABILITY_OBSERVER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class PrefTransformerInterface;

// Observes state affecting `PrefTransformerInterface::IsExtensionModifiable()`
// for a `BrowserContext` and notifies the transformer's `Observer`s on change.
class PrefModifiabilityObserver {
 public:
  PrefModifiabilityObserver(const PrefModifiabilityObserver&) = delete;
  PrefModifiabilityObserver& operator=(const PrefModifiabilityObserver&) =
      delete;
  virtual ~PrefModifiabilityObserver();

 protected:
  PrefModifiabilityObserver(PrefTransformerInterface* transformer,
                            content::BrowserContext* context);

  // Re-evaluates `IsExtensionModifiable()` and notifies the transformer if it
  // changed since construction or the last notification. Note that `this` may
  // be destroyed before this method returns if all observers unregister during
  // notification, so subclasses must not access member state afterward.
  void MaybeNotifyModifiabilityChanged();

 private:
  friend class PrefTransformerInterface;

  raw_ptr<PrefTransformerInterface> transformer_;
  raw_ptr<content::BrowserContext> context_;
  std::string browser_pref_;
  bool was_modifiable_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PREF_MODIFIABILITY_OBSERVER_H_
