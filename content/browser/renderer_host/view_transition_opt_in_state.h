// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_OPT_IN_STATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_OPT_IN_STATE_H_

#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

namespace content {
class RenderFrameHost;

// Keeps track of the Document's opt-in for same-origin cross-document
// ViewTransitions.
// This state is mirrored in the browser process so that it can be disabled
// early if if the old document did not opt-in, even if the navigation was
// initiated from the browser process.
// See:
// https://drafts.csswg.org/css-view-transitions-2/#view-transition-rule
class CONTENT_EXPORT ViewTransitionOptInState
    : public content::DocumentUserData<ViewTransitionOptInState> {
 public:
  ~ViewTransitionOptInState() override;

  void set_same_origin_opt_in(
      blink::mojom::ViewTransitionSameOriginOptIn same_origin_opt_in) {
    same_origin_opt_in_ = same_origin_opt_in;
  }
  blink::mojom::ViewTransitionSameOriginOptIn same_origin_opt_in() const {
    return same_origin_opt_in_;
  }

 private:
  explicit ViewTransitionOptInState(
      content::RenderFrameHost* render_frame_host);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  blink::mojom::ViewTransitionSameOriginOptIn same_origin_opt_in_ =
      blink::mojom::ViewTransitionSameOriginOptIn::kDisabled;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIEW_TRANSITION_OPT_IN_STATE_H_
