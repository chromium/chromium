// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/lifetime_enforcer_factories.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/types/cxx23_to_underlying.h"
#include "extensions/browser/api/offscreen/audio_lifetime_enforcer.h"
#include "extensions/browser/api/offscreen/offscreen_document_lifetime_enforcer.h"
#include "extensions/common/api/offscreen.h"

namespace extensions {

namespace {

LifetimeEnforcerFactories::TestingOverride* g_testing_override = nullptr;

// A lifetime enforcer that gives offscreen documents unbounded lifetime. This
// can be used for both reasons that may always have unbounded life (like
// testing) and for reasons for which we do not have custom lifetime enforcement
// yet.
class EmptyLifetimeEnforcer : public OffscreenDocumentLifetimeEnforcer {
 public:
  EmptyLifetimeEnforcer(OffscreenDocumentHost* offscreen_document,
                        TerminationCallback termination_callback,
                        NotifyInactiveCallback notify_inactive_callback)
      : OffscreenDocumentLifetimeEnforcer(offscreen_document,
                                          std::move(termination_callback),
                                          std::move(notify_inactive_callback)) {
  }
  ~EmptyLifetimeEnforcer() override = default;

  // OffscreenDocumentLifetimeEnforcer:
  bool IsActive() override {
    // For the empty lifetime enforcer, we always assume the document is
    // active.
    return true;
  }
};

std::unique_ptr<OffscreenDocumentLifetimeEnforcer> CreateEmptyEnforcer(
    OffscreenDocumentHost* offscreen_document,
    OffscreenDocumentLifetimeEnforcer::TerminationCallback termination_callback,
    OffscreenDocumentLifetimeEnforcer::NotifyInactiveCallback
        notify_inactive_callback) {
  return std::make_unique<EmptyLifetimeEnforcer>(
      offscreen_document, std::move(termination_callback),
      std::move(notify_inactive_callback));
}

std::unique_ptr<OffscreenDocumentLifetimeEnforcer> CreateAudioLifetimeEnforcer(
    OffscreenDocumentHost* offscreen_document,
    OffscreenDocumentLifetimeEnforcer::TerminationCallback termination_callback,
    OffscreenDocumentLifetimeEnforcer::NotifyInactiveCallback
        notify_inactive_callback) {
  return std::make_unique<AudioLifetimeEnforcer>(
      offscreen_document, std::move(termination_callback),
      std::move(notify_inactive_callback));
}

LifetimeEnforcerFactories& GetFactoriesInstance() {
  static base::NoDestructor<LifetimeEnforcerFactories> instance;
  return *instance;
}

using FactoryMethodPtr = std::unique_ptr<OffscreenDocumentLifetimeEnforcer> (*)(
    OffscreenDocumentHost*,
    OffscreenDocumentLifetimeEnforcer::TerminationCallback,
    OffscreenDocumentLifetimeEnforcer::NotifyInactiveCallback);

struct ReasonAndFactoryMethodPair {
  api::offscreen::Reason reason;
  FactoryMethodPtr factory_method;
};

// A mapping between each of the different reasons and their corresponding
// factory methods.
constexpr ReasonAndFactoryMethodPair kReasonAndFactoryMethodPairs[] = {
    {api::offscreen::Reason::kTesting, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kAudioPlayback, &CreateAudioLifetimeEnforcer},
    // The following reasons do not currently have bespoke lifetime enforcement.
    // This enforcement can be added on as-appropriate basis.
    {api::offscreen::Reason::kIframeScripting, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kDomScraping, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kBlobs, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kDomParser, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kUserMedia, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kDisplayMedia, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kWebRtc, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kClipboard, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kLocalStorage, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kWorkers, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kBatteryStatus, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kMatchMedia, &CreateEmptyEnforcer},
    {api::offscreen::Reason::kGeolocation, &CreateEmptyEnforcer}};

static_assert(std::size(kReasonAndFactoryMethodPairs) ==
                  base::to_underlying(api::offscreen::Reason::kMaxValue),
              "Factory method size does not equal reason size.");

}  // namespace

LifetimeEnforcerFactories::LifetimeEnforcerFactories() {
  InitializeFactories();
}

LifetimeEnforcerFactories::~LifetimeEnforcerFactories() = default;

// static
std::unique_ptr<OffscreenDocumentLifetimeEnforcer>
LifetimeEnforcerFactories::GetLifetimeEnforcer(
    api::offscreen::Reason reason,
    OffscreenDocumentHost* offscreen_document,
    OffscreenDocumentLifetimeEnforcer::TerminationCallback termination_callback,
    OffscreenDocumentLifetimeEnforcer::NotifyInactiveCallback
        notify_inactive_callback) {
  if (g_testing_override) {
    auto iter = g_testing_override->map().find(reason);
    if (iter != g_testing_override->map().end()) {
      return iter->second.Run(offscreen_document,
                              std::move(termination_callback),
                              std::move(notify_inactive_callback));
    }
  }

  auto& factories = GetFactoriesInstance();
  auto iter = factories.map_.find(reason);
  CHECK(iter != factories.map_.end(), base::NotFatalUntil::M130)
      << "No factory registered for: " << api::offscreen::ToString(reason);
  return iter->second.Run(offscreen_document, std::move(termination_callback),
                          std::move(notify_inactive_callback));
}

LifetimeEnforcerFactories::TestingOverride::TestingOverride() {
  DCHECK_EQ(nullptr, g_testing_override)
      << "Only a single factory override map is allowed at a time.";
  g_testing_override = this;
}

LifetimeEnforcerFactories::TestingOverride::~TestingOverride() {
  DCHECK_EQ(this, g_testing_override)
      << "Only a single factory override map is allowed at a time.";
  g_testing_override = nullptr;
}

void LifetimeEnforcerFactories::InitializeFactories() {
  for (const auto& entry : kReasonAndFactoryMethodPairs) {
    map_.emplace(entry.reason, base::BindRepeating(entry.factory_method));
  }
}

}  // namespace extensions
