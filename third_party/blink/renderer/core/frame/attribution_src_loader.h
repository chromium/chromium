// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class HTMLElement;
class KURL;
class LocalFrame;
class Resource;
class ResourceRequest;
class ResourceResponse;

struct Impression;

class CORE_EXPORT AttributionSrcLoader
    : public GarbageCollected<AttributionSrcLoader> {
 public:
  enum class RegisterContext {
    kAttributionSrc,
    kResource,
  };

  static constexpr const char* kAttributionEligibleEventSource = "event-source";
  static constexpr const char* kAttributionEligibleNavigationSource =
      "navigation-source";
  static constexpr const char* kAttributionEligibleTrigger = "trigger";
  static constexpr const char* kAttributionEligibleEventSourceAndTrigger =
      "event-source, trigger";

  explicit AttributionSrcLoader(LocalFrame* frame);
  AttributionSrcLoader(const AttributionSrcLoader&) = delete;
  AttributionSrcLoader& operator=(const AttributionSrcLoader&) = delete;
  AttributionSrcLoader(AttributionSrcLoader&& other) = delete;
  AttributionSrcLoader& operator=(AttributionSrcLoader&& other) = delete;
  ~AttributionSrcLoader();

  // Registers an attributionsrc. This method handles fetching the attribution
  // src and notifying the browser process to begin tracking it. It is a no-op
  // if the frame is not attached.
  void Register(const KURL& attribution_src, HTMLElement* element);

  // Registers an attribution resource client for the given resource if
  // the request is eligible for attribution registration. Safe to call multiple
  // times for the same `resource`. Returns whether a registration was
  // successful.
  bool MaybeRegisterAttributionHeaders(const ResourceRequest& request,
                                       const ResourceResponse& response,
                                       const Resource* resource);

  // Registers an attributionsrc which is associated with a top-level
  // navigation, for example a click on an anchor tag. Returns an Impression
  // which identifies the attributionsrc request and notifies the browser to
  // begin tracking it.
  absl::optional<Impression> RegisterNavigation(const KURL& attribution_src,
                                                HTMLElement* element = nullptr);

  void Trace(Visitor* visitor) const;

  static constexpr size_t kMaxConcurrentRequests = 30;

 private:
  class ResourceClient;

  // Represents what events are able to be registered from an attributionsrc.
  enum class SrcType { kUndetermined, kSource, kTrigger };

  ResourceClient* DoRegistration(const KURL& src_url,
                                 SrcType src_type,
                                 bool associated_with_navigation);
  void DoPrerenderingRegistration(const KURL& src_url,
                                  SrcType src_type,
                                  bool associated_with_navigation);

  // Returns whether the attribution is allowed to be registered. Devtool issue
  // might be reported if it's not allowed.
  bool UrlCanRegisterAttribution(RegisterContext context,
                                 const KURL& url,
                                 HTMLElement* element,
                                 absl::optional<uint64_t> request_id);

  void RegisterTrigger(
      mojom::blink::AttributionTriggerDataPtr trigger_data) const;

  ResourceClient* CreateAndSendRequest(const KURL& src_url,
                                       HTMLElement* element,
                                       SrcType src_type,
                                       bool associated_with_navigation);

  void LogAuditIssue(AttributionReportingIssueType issue_type,
                     const absl::optional<String>& string,
                     HTMLElement* element,
                     absl::optional<uint64_t> request_id);

  const Member<LocalFrame> local_frame_;
  size_t num_resource_clients_ = 0;
};

// Returns whether attribution is allowed, and logs devtools issues if
// registration was attempted in a context is not allowed and `log_issues` is
// set. `element` may be null.
CORE_EXPORT bool CanRegisterAttributionInContext(
    LocalFrame* frame,
    HTMLElement* element,
    absl::optional<uint64_t> request_id,
    AttributionSrcLoader::RegisterContext context,
    bool log_issues);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
