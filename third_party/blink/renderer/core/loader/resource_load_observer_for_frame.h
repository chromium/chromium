// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_LOAD_OBSERVER_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_LOAD_OBSERVER_FOR_FRAME_H_

#include <inttypes.h>

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"

namespace blink {

class CoreProbeSink;
class FrameOrImportedDocument;
class ResourceFetcherProperties;

// ResourceLoadObserver implementation associated with a frame.
class CORE_EXPORT ResourceLoadObserverForFrame final
    : public ResourceLoadObserver {
 public:
  ResourceLoadObserverForFrame(
      const FrameOrImportedDocument& frame_or_imported_document,
      const ResourceFetcherProperties& properties);
  ~ResourceLoadObserverForFrame() override;

  // ResourceLoadObserver implementation.
  void DidStartRequest(const FetchParameters&, ResourceType) override;
  void WillSendRequest(uint64_t identifier,
                       const ResourceRequest&,
                       const ResourceResponse& redirect_response,
                       ResourceType,
                       const FetchInitiatorInfo&) override;
  void DidChangePriority(uint64_t identifier,
                         ResourceLoadPriority,
                         int intra_priority_value) override;
  void DidReceiveResponse(uint64_t identifier,
                          const ResourceRequest& request,
                          const ResourceResponse& response,
                          const Resource* resource,
                          ResponseSource) override;
  void DidReceiveData(uint64_t identifier,
                      base::span<const char> chunk) override;
  void DidReceiveTransferSizeUpdate(uint64_t identifier,
                                    int transfer_size_diff) override;
  void DidDownloadToBlob(uint64_t identifier, BlobDataHandle*) override;
  void DidFinishLoading(uint64_t identifier,
                        base::TimeTicks finish_time,
                        int64_t encoded_data_length,
                        int64_t decoded_body_length,
                        bool should_report_corb_blocking) override;
  void DidFailLoading(const KURL&,
                      uint64_t identifier,
                      const ResourceError&,
                      int64_t encoded_data_length,
                      IsInternalRequest) override;
  void Trace(Visitor*) override;

 private:
  CoreProbeSink* GetProbe();
  void CountUsage(WebFeature);

  // There are some overlap between |frame_or_imported_document| and
  // |fetcher_properties_|.
  // Use this when you want to access frame, document, etc. directly.
  const Member<const FrameOrImportedDocument> frame_or_imported_document_;
  // Use this whenever possible.
  const Member<const ResourceFetcherProperties> fetcher_properties_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_LOAD_OBSERVER_FOR_FRAME_H_
