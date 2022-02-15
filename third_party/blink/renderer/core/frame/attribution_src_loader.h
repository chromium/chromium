// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"

namespace blink {

class HTMLElement;
class HTMLImageElement;
class KURL;
class LocalFrame;

class CORE_EXPORT AttributionSrcLoader
    : public GarbageCollected<AttributionSrcLoader>,
      private RawResourceClient {
 public:
  explicit AttributionSrcLoader(LocalFrame* frame);
  AttributionSrcLoader(const AttributionSrcLoader&) = delete;
  AttributionSrcLoader& operator=(const AttributionSrcLoader&) = delete;
  AttributionSrcLoader(AttributionSrcLoader&& other) = delete;
  AttributionSrcLoader& operator=(AttributionSrcLoader&& other) = delete;
  ~AttributionSrcLoader() override;

  // Registers an attribution_src. This method handles fetching the attribution
  // src and notifying the browser process to begin tracking it. Must be invoked
  // prior to `Shutdown()`, otherwise is a no-op.
  void Register(const KURL& attribution_src, HTMLImageElement* element);

  void Shutdown();

  void Trace(Visitor* visitor) const override {
    visitor->Trace(local_frame_);
    visitor->Trace(resource_data_host_map_);
    RawResourceClient::Trace(visitor);
  }

  String DebugName() const override { return "AttributionSrcLoader"; }

 private:
  // RawResourceClient:
  void ResponseReceived(Resource* resource,
                        const ResourceResponse& response) override;
  bool RedirectReceived(Resource* resource,
                        const ResourceRequest& request,
                        const ResourceResponse& response) override;
  void NotifyFinished(Resource* resource) override;

  void HandleResponseHeaders(Resource* resource,
                             const ResourceResponse& response);
  void HandleSourceRegistration(Resource* resource,
                                const ResourceResponse& response);

  void LogAuditIssue(AttributionReportingIssueType issue_type,
                     const String& string,
                     HTMLElement* element = nullptr);

  Member<LocalFrame> local_frame_;
  HeapHashMap<WeakMember<Resource>,
              mojo::Remote<mojom::blink::AttributionDataHost>>
      resource_data_host_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
