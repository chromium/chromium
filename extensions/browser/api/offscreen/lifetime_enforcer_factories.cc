// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/lifetime_enforcer_factories.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
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
    {api::offscreen::REASON_TESTING, &CreateEmptyEnforcer},
    {api::offscreen::REASON_AUDIO_PLAYBACK, &CreateAudioLifetimeEnforcer},
    // The following reasons do not currently have bespoke lifetime enforcement.
    // This enforcement can be added on as-appropriate basis.
    {api::offscreen::REASON_IFRAME_SCRIPTING, &CreateEmptyEnforcer},
    {api::offscreen::REASON_DOM_SCRAPING, &CreateEmptyEnforcer},
    {api::offscreen::REASON_BLOBS, &CreateEmptyEnforcer},
    {api::offscreen::REASON_DOM_PARSER, &CreateEmptyEnforcer},
    {api::offscreen::REASON_USER_MEDIA, &CreateEmptyEnforcer},
    {api::offscreen::REASON_DISPLAY_MEDIA, &CreateEmptyEnforcer},
    {api::offscreen::REASON_WEB_RTC, &CreateEmptyEnforcer},
    {api::offscreen::REASON_CLIPBOARD, &CreateEmptyEnforcer},
};

static_assert(std::size(kReasonAndFactoryMethodPairs) ==
                  api::offscreen::REASON_LAST,
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
    if (iter != g_testing_override->map().end())
      return iter->second.Run(offscreen_document,
                              std::move(termination_callback),
                              std::move(notify_inactive_callback));
  }

  auto& factories = GetFactoriesInstance();
  auto iter = factories.map_.find(reason);
  DCHECK(iter != factories.map_.end())
      << "No factory registered for: " << reason;
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
