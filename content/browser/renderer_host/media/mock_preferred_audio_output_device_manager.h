// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_PREFERRED_AUDIO_OUTPUT_DEVICE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_PREFERRED_AUDIO_OUTPUT_DEVICE_MANAGER_H_

#include "content/browser/renderer_host/media/preferred_audio_output_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockPreferredAudioOutputDeviceManager
    : public PreferredAudioOutputDeviceManager {
 public:
  MockPreferredAudioOutputDeviceManager();
  ~MockPreferredAudioOutputDeviceManager() override;

  MOCK_METHOD(void,
              SetPreferredSinkId,
              (const GlobalRenderFrameHostToken& main_frame_token,
               const std::string& raw_device_id,
               SetPreferredSinkIdCallback callback),
              (override));
  MOCK_METHOD(void,
              AddSwitcher,
              (const GlobalRenderFrameHostToken& main_frame_token,
               AudioOutputDeviceSwitcher* device_switcher),
              (override));
  MOCK_METHOD(void,
              RemoveSwitcher,
              (const GlobalRenderFrameHostToken& main_frame_token,
               AudioOutputDeviceSwitcher* device_switcher),
              (override));
  MOCK_METHOD(const std::string&,
              GetPreferredSinkId,
              (const GlobalRenderFrameHostToken& main_frame_token),
              (override));
  MOCK_METHOD(void,
              UnregisterMainFrameOnUIThread,
              (RenderFrameHost * render_frame_host),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MOCK_PREFERRED_AUDIO_OUTPUT_DEVICE_MANAGER_H_
