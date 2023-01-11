// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_OFFSCREEN_LIFETIME_ENFORCER_FACTORIES_H_
#define EXTENSIONS_BROWSER_API_OFFSCREEN_LIFETIME_ENFORCER_FACTORIES_H_

#include <map>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "extensions/browser/api/offscreen/offscreen_document_lifetime_enforcer.h"
#include "extensions/common/api/offscreen.h"

namespace extensions {

// A registry of factories to create different lifetime enforcers for
// offscreen documents.
class LifetimeEnforcerFactories {
 public:
  using Factory = base::RepeatingCallback<
      std::unique_ptr<OffscreenDocumentLifetimeEnforcer>(
          OffscreenDocumentHost*,
          OffscreenDocumentLifetimeEnforcer::TerminationCallback,
          OffscreenDocumentLifetimeEnforcer::NotifyInactiveCallback)>;
  using FactoryMap = std::map<api::offscreen::Reason, Factory>;

  // A helper utility to allow overriding the lifetime enforcement of different
  // reasons for testing purposes.
  class TestingOverride {
   public:
    TestingOverride();
    TestingOverride(const TestingOverride&) = delete;
    TestingOverride& operator=(const TestingOverride&) = delete;
    ~TestingOverride();

    FactoryMap& map() { return map_; }

   private:
    FactoryMap map_;
  };

  // Creates a new instance of the appropriate lifetime enforcer for the given
  // `reason`.
  static std::unique_ptr<OffscreenDocumentLifetimeEnforcer> GetLifetimeEnforcer(
      api::offscreen::Reason reason,
      OffscreenDocumentHost* offscreen_document,
      OffscreenDocumentLifetimeEnforcer::TerminationCallback
          termination_callback,
      OffscreenDocumentLifetimeEnforcer::NotifyInactiveCallback
          activity_changed_callback);

 private:
  friend class base::NoDestructor<LifetimeEnforcerFactories>;

  LifetimeEnforcerFactories();
  ~LifetimeEnforcerFactories();

  void InitializeFactories();

  // A map of all reasons for creating an offscreen document to their registered
  // lifetime enforcer.
  FactoryMap map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_OFFSCREEN_LIFETIME_ENFORCER_FACTORIES_H_
