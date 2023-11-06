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
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ScopedApiStateForTesting =
    ::content::AttributionOsLevelManager::ScopedApiStateForTesting;

using ApiState = AttributionOsLevelManager::ApiState;

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

}  // namespace

// static
bool AttributionOsLevelManager::ShouldUseOsWebSource(
    GlobalRenderFrameHostId render_frame_id) {
  return GetContentClient()
      ->browser()
      ->ShouldUseOsWebSourceAttributionReporting(
          RenderFrameHost::FromID(render_frame_id));
}

// static
bool AttributionOsLevelManager::ShouldUseOsWebTrigger(
    GlobalRenderFrameHostId render_frame_id) {
  return GetContentClient()
      ->browser()
      ->ShouldUseOsWebTriggerAttributionReporting(
          RenderFrameHost::FromID(render_frame_id));
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
ApiState AttributionOsLevelManager::GetApiState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());
  return g_state.value_or(ApiState::kDisabled);
}

// static
void AttributionOsLevelManager::SetApiState(absl::optional<ApiState> state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetSequenceChecker());

  ApiState old_state = GetApiState();
  g_state = state;
  ApiState new_state = GetApiState();

  base::UmaHistogramEnumeration("Conversions.AttributionOsLevelApiState",
                                new_state);

  if (old_state == new_state) {
    return;
  }

  WebContentsImpl::UpdateAttributionSupportAllRenderers();
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

void NoOpAttributionOsLevelManager::Register(OsRegistration registration,
                                             bool is_debug_key_allowed,
                                             RegisterCallback callback) {
  std::move(callback).Run(registration, false);
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
