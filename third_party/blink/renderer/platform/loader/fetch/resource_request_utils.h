// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_UTILS_H_

#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

// This method simply takes in information about a ResourceRequest, and returns
// if the resource should be loaded in parallel (incremental) or sequentially
// for protocols that support multiplexing and HTTP extensible priorities
// (RFC 9218).
// Most content types can be operated on with partial data (document parsing,
// images, media, etc) but a few need to be complete before they can be
// processed.
bool BLINK_PLATFORM_EXPORT ShouldLoadIncremental(ResourceType type);

// Returns the adjusted version of `priority` according to the given
// `fetch_priority_hint` and `render_blocking_behavior`.
ResourceLoadPriority BLINK_PLATFORM_EXPORT
AdjustPriorityWithPriorityHintAndRenderBlocking(
    ResourceLoadPriority priority,
    ResourceType type,
    mojom::blink::FetchPriorityHint fetch_priority_hint,
    RenderBlockingBehavior render_blocking_behavior);

// A callback type to compute ResourceLoadPriority for the ResourceRequest from
// the given FetchParameters. Returns the computed result.
using ResourceLoadPriorityCalculator =
    base::OnceCallback<ResourceLoadPriority(const FetchParameters&)>;

// A callback type to enable tracing using the given ResourceRequest.
using ResourceRequestTraceCallback =
    base::OnceCallback<void(const ResourceRequest&)>;

// Returns absl::nullopt if loading the ResourceRequest in `params` is not
// blocked. Otherwise, returns a blocked reason.
// This method may modify the ResourceRequest in `params` according to
// `context` and `resource_type`.
//
// `virtual_time_pauser` may be set by this method.
//
// `compute_load_priority_callback` is used to compute the priority of the
// ResourceRequest if not yet set before calling this function.
//
// `trace_callback` is executed at some point to enable tracing within this
// function.
//
// `bundle_url_for_uuid_resources` is an optional bundle URL for
// uuid-in-package: resources for security checks. Should only be set when the
// request is WebBundle.
absl::optional<ResourceRequestBlockedReason> BLINK_PLATFORM_EXPORT
PrepareResourceRequest(
    const ResourceType& resource_type,
    const FetchClientSettingsObject& fetch_client_settings_object,
    FetchParameters& params,
    FetchContext& context,
    WebScopedVirtualTimePauser& virtual_time_pauser,
    ResourceLoadPriorityCalculator compute_load_priority_callback =
        base::NullCallback(),
    ResourceRequestTraceCallback trace_callback = base::NullCallback(),
    const KURL& bundle_url_for_uuid_resources = KURL());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_REQUEST_UTILS_H_
