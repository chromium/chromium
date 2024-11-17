// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SELECT_AUDIO_OUTPUT_REQUEST_H_
#define CONTENT_PUBLIC_BROWSER_SELECT_AUDIO_OUTPUT_REQUEST_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

struct AudioOutputDeviceInfo {
  std::string device_id;
  std::string label;
};

enum class SelectAudioOutputError {
  kNotSupported,
  kNoPermission,
  kUserCancelled,
  kOtherError
};

using SelectAudioOutputCallback = base::OnceCallback<void(
    base::expected<std::string, content::SelectAudioOutputError>)>;

class CONTENT_EXPORT SelectAudioOutputRequest {
 public:
  SelectAudioOutputRequest(
      content::GlobalRenderFrameHostId render_frame_host_id,
      const std::vector<AudioOutputDeviceInfo>& audio_output_devices);

  ~SelectAudioOutputRequest();

  const GlobalRenderFrameHostId& render_frame_host_id() const {
    return render_frame_host_id_;
  }
  const std::vector<AudioOutputDeviceInfo>& audio_output_devices() const {
    return audio_output_devices_;
  }

 private:
  GlobalRenderFrameHostId render_frame_host_id_;
  std::vector<AudioOutputDeviceInfo> audio_output_devices_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SELECT_AUDIO_OUTPUT_REQUEST_H_
