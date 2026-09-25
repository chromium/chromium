// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_IMPL_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/callback_list.h"
#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"

class PrefService;

namespace web {
class ExtensionController;
enum class UniversalOptOutState;
}  // namespace web

// Implementation of `ExtensionService` managing web extensions for the profile.
class ExtensionServiceImpl final : public ExtensionService {
 public:
  // Creates an `ExtensionServiceImpl` with preferences and the injected
  // `extension_controller`. Call `Initialize(state)` after creation
  // to begin loading extensions if applicable.
  ExtensionServiceImpl(
      PrefService& pref_service,
      std::unique_ptr<web::ExtensionController> extension_controller)
      API_AVAILABLE(ios(18.4));

  ExtensionServiceImpl(const ExtensionServiceImpl&) = delete;
  ExtensionServiceImpl& operator=(const ExtensionServiceImpl&) = delete;

  ~ExtensionServiceImpl() override;

  // KeyedService:
  void Shutdown() override;

  // ExtensionService:
  void Initialize(web::UniversalOptOutState state) override;
  web::ExtensionController* GetExtensionController() const override
      API_AVAILABLE(ios(18.4));
  bool IsReady() const override;
  bool WebExtensionsWereLoadedAtStartup() const override;
  base::CallbackListSubscription RunWhenReady(
      base::OnceClosure callback) override;

 private:
  // Called when the extension loading callback finishes.
  void OnExtensionLoaded(bool success);

  // Called when loading the extension times out.
  void OnExtensionLoadTimeout();

  // Marks the service as ready during startup and notifies all waiting
  // callbacks. Readiness is solely for startup and is triggered only once.
  void NotifyReady();

  // Called when the `kUniversalOptOutEnabled` preference changes. Updates the
  // loaded state of the extension without modifying the startup readiness
  // state.
  void OnOptOutPrefChanged();

  // The `PrefService` used to query and observe opt-out preferences.
  const raw_ref<PrefService> pref_service_;

  // The `ExtensionController` owned by this service.
  std::unique_ptr<web::ExtensionController> extension_controller_;

  // Registrar for preference changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Timer for loading timeout.
  base::OneShotTimer loading_timer_;

  // Whether the service has completed loading initial extensions.
  bool is_ready_ = false;

  // Whether the extension started loading at startup.
  bool extension_load_started_at_startup_ = false;

  // Whether an extension is currently being loaded.
  bool is_loading_ = false;

  // List of callbacks waiting for the service to be ready.
  base::OnceClosureList ready_callbacks_;

  // Timestamp when initialization started.
  base::TimeTicks initialization_start_time_;

  // Timestamp when loading the extension started.
  base::TimeTicks extension_load_start_time_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ExtensionServiceImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_IMPL_H_
