// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_CONTAINER_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "content/browser/speculation_rules/prefetch/prefetch_status.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

// This class contains the state for a request to prefetch a specific URL.
class CONTENT_EXPORT PrefetchContainer {
 public:
  PrefetchContainer(
      const GlobalRenderFrameHostId& referring_render_frame_host_id,
      const GURL& url,
      const PrefetchType& prefetch_type);
  ~PrefetchContainer();

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // Defines the key to uniquely identify a prefetch.
  using Key = std::pair<GlobalRenderFrameHostId, GURL>;
  Key GetPrefetchContainerKey() const {
    return std::make_pair(referring_render_frame_host_id_, url_);
  }

  // The ID of the render frame host that triggered the prefetch.
  GlobalRenderFrameHostId GetReferringRenderFrameHostId() const {
    return referring_render_frame_host_id_;
  }

  // The URL that will potentially be prefetched.
  GURL GetURL() const { return url_; }

  // The type of this prefetch. Controls how the prefetch is handled.
  const PrefetchType& GetPrefetchType() const { return prefetch_type_; }

  base::WeakPtr<PrefetchContainer> GetWeakPtr() const {
    return weak_method_factory_.GetWeakPtr();
  }

  // The status of the current prefetch. Note that |HasPrefetchStatus| will be
  // initially false until |SetPrefetchStatus| is called.
  void SetPrefetchStatus(PrefetchStatus prefetch_status) {
    prefetch_status_ = prefetch_status;
  }
  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchStatus GetPrefetchStatus() const;

 private:
  // The ID of the render frame host that triggered the prefetch.
  GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The URL that will potentially be prefetched
  GURL url_;

  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  PrefetchType prefetch_type_;

  // The current status, if any, of the prefetch.
  absl::optional<PrefetchStatus> prefetch_status_;

  base::WeakPtrFactory<PrefetchContainer> weak_method_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_CONTAINER_H_
