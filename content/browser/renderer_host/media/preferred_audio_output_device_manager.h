// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_PREFERRED_AUDIO_OUTPUT_DEVICE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_PREFERRED_AUDIO_OUTPUT_DEVICE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/media/audio_output_authorization_handler.h"
#include "content/public/browser/global_routing_id.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"

namespace content {

class AudioOutputDeviceSwitcher {
 public:
  virtual ~AudioOutputDeviceSwitcher() = default;

  virtual void SwitchAudioOutputDeviceId(const std::string& device_id) = 0;
};

using SetPreferredSinkIdCallback =
    base::OnceCallback<void(media::OutputDeviceStatus)>;

// This class keeps track of which frame tree each switcher belongs to,
// and applies preferred output device changes to all switchers belonging
// to a tree of a given main frame.

// It allows switching the audio output for the page, handling both
// main frames and subframes.
class CONTENT_EXPORT PreferredAudioOutputDeviceManager {
 public:
  virtual ~PreferredAudioOutputDeviceManager() = default;

  // Sets the preferred sink id for the main frame and all its subframes.
  // At this moment, all belonging to the frame tree of the main frame
  // identified by `main_frame_token` will be updated with preferred sink id.
  // `raw_device_id` is the id of the sink to be set as the preferred sink. It
  // is a raw device id, which could be acquired by
  // AudioOutputAuthorizationHandler.
  // `callback` is the callback to be called with the status of the sink id set.
  virtual void SetPreferredSinkId(
      const GlobalRenderFrameHostToken& main_frame_token,
      const std::string& raw_device_id,
      SetPreferredSinkIdCallback callback) = 0;
  // Adds the device switcher to the device switchers list.
  // `main_frame_token` is the token of the main frame to whose frame tree
  // the device switcher belongs.
  // `device_switcher` is the device switcher to be added to the device
  // switchers list. The caller should call the RemoveSwitcher API if the
  // device switcher is no longer needed or the object is being destroyed
  // in order to prevent dangling pointers.
  virtual void AddSwitcher(const GlobalRenderFrameHostToken& main_frame_token,
                           AudioOutputDeviceSwitcher* device_switcher) = 0;

  // Removes the device switcher from the device switchers list.
  // `main_frame_token` is the token of the main frame to whose frame tree
  // the device switcher belongs.
  // `device_switcher` is the device switcher to be removed from the
  // device switchers list.
  // It is expected that the API is called with the same parameters of
  // AddSwitcher().
  virtual void RemoveSwitcher(
      const GlobalRenderFrameHostToken& main_frame_token,
      AudioOutputDeviceSwitcher* device_switcher) = 0;

  // Returns the preferred device id for the frame if it is
  // registered, or an empty device id otherwise.
  // `main_frame_token` is the token of the main frame to whose frame tree
  // the device switcher belongs.
  virtual const std::string& GetPreferredSinkId(
      const GlobalRenderFrameHostToken& main_frame_token) = 0;

  // Unregisters the main frame and all its subframes with their switchers.
  // `render_frame_host` is the frame for which the entry is to be
  // removed. It is expected that it is the main frame,
  // otherwise it does nothing. It should be called from the UI thread.
  virtual void UnregisterMainFrameOnUIThread(
      RenderFrameHost* render_frame_host) = 0;
};

// The class is expected to be created and destroyed on the IO thread as
// as singleton per the browser instance.

// Threading model: Most of the APIs are expected to be called on the IO thread
// except for the `UnregisterMainFrameOnUIThread` which is on the UI thread.
class CONTENT_EXPORT PreferredAudioOutputDeviceManagerImpl final
    : public PreferredAudioOutputDeviceManager {
 public:
  explicit PreferredAudioOutputDeviceManagerImpl();

  ~PreferredAudioOutputDeviceManagerImpl() override;

  void SetPreferredSinkId(const GlobalRenderFrameHostToken& main_frame_token,
                          const std::string& hashed_sink_id,
                          SetPreferredSinkIdCallback callback) override;
  void AddSwitcher(const GlobalRenderFrameHostToken& main_frame_token,
                   AudioOutputDeviceSwitcher* device_switcher) override;
  void RemoveSwitcher(const GlobalRenderFrameHostToken& main_frame_token,
                      AudioOutputDeviceSwitcher* device_switcher) override;
  const std::string& GetPreferredSinkId(
      const GlobalRenderFrameHostToken& main_frame_token) override;
  void UnregisterMainFrameOnUIThread(
      RenderFrameHost* render_frame_host) override;

 private:
  void UnregisterMainFrameOnIOThread(
      const GlobalRenderFrameHostToken& main_frame_token);
  void AddNewConfigEntry(const GlobalRenderFrameHostToken& main_frame_token,
                         const std::string& sink_id,
                         AudioOutputDeviceSwitcher* device_switcher);

  class MainFramePreferredSinkIdConfig;
  MainFramePreferredSinkIdConfig* FindSinkIdConfig(
      const GlobalRenderFrameHostToken& main_frame_token) const;

  // The MainFramePreferredSinkIdConfig list. It should
  // be accessed only in the IO thread.
  std::vector<std::unique_ptr<MainFramePreferredSinkIdConfig>>
      preferred_sink_id_configs_;

  base::WeakPtrFactory<PreferredAudioOutputDeviceManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_PREFERRED_AUDIO_OUTPUT_DEVICE_MANAGER_H_
