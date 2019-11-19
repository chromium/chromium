// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/back_forward_cache_util.h"

#include <map>
#include <set>

#include "content/browser/frame_host/back_forward_cache_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

bool IsInBackForwardCache(RenderFrameHost* render_frame_host) {
  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  return rfhi->is_in_back_forward_cache();
}

class BackForwardCacheDisabledTester::Impl
    : public BackForwardCacheTestDelegate {
 public:
  bool IsDisabledForFrameWithReason(GlobalFrameRoutingId id,
                                    base::StringPiece reason) {
    return disable_reasons_[id].count(std::string(reason)) != 0;
  }

  void OnDisabledForFrameWithReason(GlobalFrameRoutingId id,
                                    base::StringPiece reason) override {
    disable_reasons_[id].insert(std::string(reason));
  }

 private:
  std::map<GlobalFrameRoutingId, std::set<std::string>> disable_reasons_;
};

BackForwardCacheDisabledTester::BackForwardCacheDisabledTester()
    : impl_(std::make_unique<Impl>()) {}

BackForwardCacheDisabledTester::~BackForwardCacheDisabledTester() {}

bool BackForwardCacheDisabledTester::IsDisabledForFrameWithReason(
    int process_id,
    int frame_routing_id,
    base::StringPiece reason) {
  return impl_->IsDisabledForFrameWithReason(
      GlobalFrameRoutingId{process_id, frame_routing_id}, reason);
}

}  // namespace content
