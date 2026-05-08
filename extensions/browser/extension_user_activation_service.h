// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_USER_ACTIVATION_SERVICE_H_
#define EXTENSIONS_BROWSER_EXTENSION_USER_ACTIVATION_SERVICE_H_

#include <map>

#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Tracks transient user activation state for extensions.
// This service is notified when an extension is activated by a user gesture
// (such as during API calls or event dispatching) and maintains that activation
// state for a short duration.
class ExtensionUserActivationService : public KeyedService {
 public:
  ExtensionUserActivationService();
  ~ExtensionUserActivationService() override;

  ExtensionUserActivationService(const ExtensionUserActivationService&) =
      delete;
  ExtensionUserActivationService& operator=(
      const ExtensionUserActivationService&) = delete;

  static ExtensionUserActivationService* Get(content::BrowserContext* context);

  // Notifies the service that the extension was activated by a user gesture.
  void NotifyUserActivation(const ExtensionId& extension_id);

  // Returns true if the extension has a transient activation (i.e. it was
  // activated by a user gesture within the last 5 seconds).
  bool HasTransientActivation(const ExtensionId& extension_id) const;

 private:
  void RemoveActivation(const ExtensionId& extension_id);

  std::map<ExtensionId, base::OneShotTimer> user_activation_timers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_USER_ACTIVATION_SERVICE_H_
