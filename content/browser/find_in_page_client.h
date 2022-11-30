// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIND_IN_PAGE_CLIENT_H_
#define CONTENT_BROWSER_FIND_IN_PAGE_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

namespace content {

class RenderFrameHostImpl;
class FindRequestManager;

// Per-frame client of FindInPage, owned by FindRequestManager.
// Keeps track of the current match count for the frame.
class CONTENT_EXPORT FindInPageClient : public blink::mojom::FindInPageClient {
 public:
  FindInPageClient(FindRequestManager* find_request_manager,
                   RenderFrameHostImpl* rfh);

  FindInPageClient(const FindInPageClient&) = delete;
  FindInPageClient& operator=(const FindInPageClient&) = delete;

  ~FindInPageClient() override;

#if BUILDFLAG(IS_ANDROID)
  void ActivateNearestFindResult(int request_id, const gfx::PointF& point);
#endif

  // Current number of matches for this frame.
  int number_of_matches() { return number_of_matches_; }

  // blink::mojom::FindInPageClient overrides

  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      blink::mojom::FindMatchUpdateType update_type) override;

  void SetActiveMatch(int request_id,
                      const gfx::Rect& active_match_rect,
                      int active_match_ordinal,
                      blink::mojom::FindMatchUpdateType update_type) override;

 private:
  void HandleUpdateType(int request_id,
                        blink::mojom::FindMatchUpdateType update_type);
  const raw_ptr<RenderFrameHostImpl> frame_;
  const raw_ptr<FindRequestManager> find_request_manager_;
  mojo::Receiver<blink::mojom::FindInPageClient> receiver_{this};
  int number_of_matches_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIND_IN_PAGE_CLIENT_H_
