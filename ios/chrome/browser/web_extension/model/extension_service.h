// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <vector>

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/timer/timer.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"

class PrefService;

namespace universal_optout {
class UniversalOptOutService;
}  // namespace universal_optout

namespace web {
class ExtensionController;
}  // namespace web

// A profile-keyed service managing web extensions for the profile.
class ExtensionService : public KeyedService {
 public:
  // Creates an `ExtensionService` with preferences, opt-out service, and
  // the injected `extension_controller`. Call `Initialize()` after creation
  // to begin loading extensions.
  ExtensionService(
      PrefService* pref_service,
      universal_optout::UniversalOptOutService* universal_optout_service,
      std::unique_ptr<web::ExtensionController> extension_controller)
      API_AVAILABLE(ios(18.4));

  ExtensionService(const ExtensionService&) = delete;
  ExtensionService& operator=(const ExtensionService&) = delete;

  ~ExtensionService() override;

  // KeyedService:
  void Shutdown() override;

  // Initializes the service, registers preference observers, and initiates
  // loading of extensions if applicable.
  void Initialize();

  // Returns the `web::ExtensionController` owned by this service.
  web::ExtensionController* GetExtensionController() const
      API_AVAILABLE(ios(18.4));

  // Returns whether the initial extensions have finished loading (or if there
  // are no extensions to load) during startup. Readiness is solely for startup;
  // once the service is ready, it remains ready for the remainder of its
  // lifetime regardless of subsequent preference or extension state changes.
  bool IsReady() const;

  // Registers `callback` to be invoked when the extension service finishes
  // startup initialization. If already ready, `callback` is called immediately.
  void RunWhenReady(base::OnceClosure callback);

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
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The `UniversalOptOutService` used to query user consent and eligibility.
  raw_ptr<universal_optout::UniversalOptOutService> universal_optout_service_ =
      nullptr;

  // The `ExtensionController` owned by this service.
  std::unique_ptr<web::ExtensionController> extension_controller_;

  // Registrar for preference changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Timer for loading timeout.
  base::OneShotTimer loading_timer_;

  // Whether the service has completed loading initial extensions.
  bool is_ready_ = false;

  // Whether an extension is currently being loaded.
  bool is_loading_ = false;

  // List of callbacks waiting for the service to be ready.
  std::vector<base::OnceClosure> ready_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ExtensionService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_H_
