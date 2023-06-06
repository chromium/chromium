// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_os_level_manager.h"

#include <utility>

#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ScopedApiStateForTesting =
    ::content::AttributionOsLevelManager::ScopedApiStateForTesting;

using ApiState = ::content::AttributionOsLevelManager::ApiState;

#if DCHECK_IS_ON()
const base::SequenceChecker& GetSequenceChecker() {
  static base::NoDestructor<base::SequenceChecker> checker;
  return *checker;
}
#endif

// This flag is per device and can only be changed by the OS. Currently we don't
// observe setting changes on the device and the flag is only initialized once
// on startup. The value may vary in tests.
absl::optional<ApiState> g_state GUARDED_BY_CONTEXT(GetSequenceChecker());

ApiState GetApiState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());
  return g_state.value_or(ApiState::kDisabled);
}

}  // namespace

// static
network::mojom::AttributionSupport AttributionOsLevelManager::GetSupport() {
  bool is_web_allowed =
      GetContentClient()->browser()->IsWebAttributionReportingAllowed();
  switch (GetApiState()) {
    case ApiState::kDisabled:
      return is_web_allowed ? network::mojom::AttributionSupport::kWeb
                            : network::mojom::AttributionSupport::kNone;
    case ApiState::kEnabled:
      return is_web_allowed ? network::mojom::AttributionSupport::kWebAndOs
                            : network::mojom::AttributionSupport::kOs;
  }
}

// static
bool AttributionOsLevelManager::ShouldUseOsWebSource() {
  return GetContentClient()
      ->browser()
      ->ShouldUseOsWebSourceAttributionReporting();
}

// static
bool AttributionOsLevelManager::ShouldInitializeApiState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());
  if (g_state.has_value()) {
    return false;
  }
  g_state.emplace(ApiState::kDisabled);
  return true;
}

// static
void AttributionOsLevelManager::SetApiState(absl::optional<ApiState> state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());

  network::mojom::AttributionSupport old_support = GetSupport();
  g_state = state;
  network::mojom::AttributionSupport new_support = GetSupport();

  base::UmaHistogramEnumeration("Conversions.AttributionSupport", new_support);

  if (old_support == new_support) {
    return;
  }

  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->SetAttributionReportingSupport(new_support);
  }
}

ScopedApiStateForTesting::ScopedApiStateForTesting(
    absl::optional<ApiState> state)
    : previous_(g_state) {
  SetApiState(state);
}

ScopedApiStateForTesting::~ScopedApiStateForTesting() {
  SetApiState(previous_);
}

NoOpAttributionOsLevelManager::~NoOpAttributionOsLevelManager() = default;

void NoOpAttributionOsLevelManager::Register(
    const OsRegistration&,
    bool is_debug_key_allowed,
    base::OnceCallback<void(bool success)> callback) {
  std::move(callback).Run(false);
}

void NoOpAttributionOsLevelManager::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    const std::set<url::Origin>& origins,
    const std::set<std::string>& domains,
    BrowsingDataFilterBuilder::Mode mode,
    bool delete_rate_limit_data,
    base::OnceClosure done) {
  std::move(done).Run();
}

}  // namespace content
