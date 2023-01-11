// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBRTC_LOG_H_
#define CONTENT_PUBLIC_BROWSER_WEBRTC_LOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "media/media_buildflags.h"

namespace content {

class CONTENT_EXPORT WebRtcLog {
 public:
  WebRtcLog() = delete;
  WebRtcLog(const WebRtcLog&) = delete;
  WebRtcLog& operator=(const WebRtcLog&) = delete;

  // When set, |callback| receives log messages regarding, for example, media
  // devices (webcams, mics, etc) that were initially requested in the render
  // process associated with the RenderProcessHost with |render_process_id|.
  static void SetLogMessageCallback(
      int render_process_id,
      base::RepeatingCallback<void(const std::string&)> callback);
  static void ClearLogMessageCallback(int render_process_id);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBRTC_LOG_H_
