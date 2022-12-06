// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/conversions/attribution_reporting.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

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
  static constexpr const char* kAttributionEligibleEventSource = "event-source";
  static constexpr const char* kAttributionEligibleNavigationSource =
      "navigation-source";
  static constexpr const char* kAttributionEligibleTrigger = "trigger";

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
  absl::optional<Impression> RegisterNavigation(
      const KURL& attribution_src,
      mojom::blink::AttributionNavigationType nav_type,
      HTMLElement* element = nullptr);

  // Returns true if `url` can be used as an attributionsrc: its scheme is HTTP
  // or HTTPS, its origin is potentially trustworthy, the document's permission
  // policy supports Attribution Reporting, the window's context is secure, and
  // the Attribution Reporting runtime-enabled feature is enabled.
  //
  // Reports a DevTools issue using `element` and `request_id` otherwise, if
  // `log_issues` is true.
  [[nodiscard]] bool CanRegister(const KURL& url,
                                 HTMLElement* element,
                                 absl::optional<uint64_t> request_id,
                                 bool log_issues = true);

  void Trace(Visitor* visitor) const;

  // Returns proper value to populate `Attribution-Reporting-Support` request
  // header. If OS-level attribution is supported, returns "web, os", otherwise
  // returns "web".
  AtomicString GetSupportHeader() const;

  static constexpr size_t kMaxConcurrentRequests = 30;

 private:
  class ResourceClient;

  ResourceClient* DoRegistration(
      const KURL& src_url,
      mojom::blink::AttributionRegistrationType,
      absl::optional<mojom::blink::AttributionNavigationType> nav_type);

  // Returns the reporting origin corresponding to `url` if its protocol is in
  // the HTTP family, its origin is potentially trustworthy, and attribution is
  // allowed. Returns `absl::nullopt` otherwise, and reports a DevTools issue
  // using `element` and `request_id if `log_issues` is true.
  absl::optional<attribution_reporting::SuitableOrigin>
  ReportingOriginForUrlIfValid(const KURL& url,
                               HTMLElement* element,
                               absl::optional<uint64_t> request_id,
                               bool log_issues = true);

  ResourceClient* CreateAndSendRequest(
      const KURL& src_url,
      HTMLElement* element,
      mojom::blink::AttributionRegistrationType,
      absl::optional<mojom::blink::AttributionNavigationType> nav_type);

  // Returns whether OS-level attribution is supported.
  bool HasOsSupport() const;

  void RegisterAttributionHeaders(
      mojom::blink::AttributionRegistrationType,
      attribution_reporting::SuitableOrigin reporting_origin,
      const AtomicString& source_json,
      const AtomicString& trigger_json,
      uint64_t request_id);

  const Member<LocalFrame> local_frame_;
  size_t num_resource_clients_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
