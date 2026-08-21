// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FRAME_EVICTION_OPT_OUT_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_FRAME_EVICTION_OPT_OUT_CLIENT_H_

#include "base/types/pass_key.h"
#include "content/common/content_export.h"

class WebUIToolbarWebView;

namespace waap {
class PrewarmHelper;
}  // namespace waap

namespace content {

// Add your class to the friend list to obtain a PassKey for
// WebContents::OptOutFrameEviction(). This API is intended only for a very
// limited set of WebContents. Callers must carefully evaluate the trade-offs
// before using this API.
//
// Opting out of frame eviction provides:
// - Instant frame presentation when resuming from a hidden state.
// - Reduced tab switching time, because an opted-out frame does not compete
//   with tabs for a limited number of frame slots in
//   viz::FrameEvictionManager.
//
// At the cost of:
// - Increased memory usage.
// - Potential for increased out-of-memory (OOM) crashes.
class CONTENT_EXPORT FrameEvictionOptOutClient {
 public:
  using PassKey = base::PassKey<FrameEvictionOptOutClient>;

  FrameEvictionOptOutClient() = delete;

  static PassKey GetPassKeyForTesting();

 private:
  friend class ::WebUIToolbarWebView;
  friend class waap::PrewarmHelper;

  static PassKey GetPassKey();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FRAME_EVICTION_OPT_OUT_CLIENT_H_
