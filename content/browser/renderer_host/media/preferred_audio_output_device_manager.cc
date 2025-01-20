// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/preferred_audio_output_device_manager.h"

#include <algorithm>

#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_device_description.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"

namespace content {

// This class controls preferred sink id for all switchers registered as
// belonging to a tree of a given main frame.
// MainFramePreferredSinkIdConfig entry will be added during SetPreferredSinkId
// or AddSwitcher whichever comes first for the given main frame token. It will
// be removed by the UnregisterMainFrameOnUIThread call for the given main frame
// token.
class PreferredAudioOutputDeviceManagerImpl::MainFramePreferredSinkIdConfig {
 public:
  explicit MainFramePreferredSinkIdConfig(
      const GlobalRenderFrameHostToken& main_frame_global_id,
      const std::string& sink_id);
  ~MainFramePreferredSinkIdConfig();

  MainFramePreferredSinkIdConfig(const MainFramePreferredSinkIdConfig&) =
      delete;
  MainFramePreferredSinkIdConfig& operator=(
      const MainFramePreferredSinkIdConfig&) = delete;

  const std::string& preferred_sink_id() const { return preferred_sink_id_; }

  const GlobalRenderFrameHostToken& main_frame_global_id() const {
    return main_frame_global_id_;
  }

  void AddDeviceSwitcher(AudioOutputDeviceSwitcher* device_switcher) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    CHECK(std::find(device_switchers_.begin(), device_switchers_.end(),
                    device_switcher) == device_switchers_.end());
    device_switcher->SwitchAudioOutputDeviceId(preferred_sink_id());
    device_switchers_.push_back(device_switcher);
  }

  void RemoveDeviceSwitcher(AudioOutputDeviceSwitcher* device_switcher) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(device_switcher);
    auto it = std::remove_if(
        device_switchers_.begin(), device_switchers_.end(),
        [device_switcher](const raw_ptr<AudioOutputDeviceSwitcher>& ptr) {
          return ptr.get() == device_switcher;
        });
    if (it != device_switchers_.end()) {
      device_switchers_.erase(it, device_switchers_.end());
    }
  }

  void SetPreferredSinkId(const std::string& new_device_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (preferred_sink_id() == new_device_id) {
      return;
    }

    preferred_sink_id_ = new_device_id;

    // Switch the audio output device for all the registered device switchers.
    for (size_t i = 0; i < device_switchers_.size(); ++i) {
      device_switchers_[i]->SwitchAudioOutputDeviceId(preferred_sink_id());
    }
  }

 private:
  // The main frame's GlobalRenderFrameHostToken.
  const GlobalRenderFrameHostToken main_frame_global_id_;

  // The preferred sink id for the top-level page as well as all the sub
  // frames.
  std::string preferred_sink_id_;

  // As contract, the client should call RemoveSwitcher to prevent
  // any dangling pointer issue.
  std::vector<raw_ptr<AudioOutputDeviceSwitcher>> device_switchers_;
};

PreferredAudioOutputDeviceManagerImpl::MainFramePreferredSinkIdConfig::
    MainFramePreferredSinkIdConfig(
        const GlobalRenderFrameHostToken& main_frame_global_id,
        const std::string& sink_id)
    : main_frame_global_id_(main_frame_global_id), preferred_sink_id_(sink_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

PreferredAudioOutputDeviceManagerImpl::MainFramePreferredSinkIdConfig::
    ~MainFramePreferredSinkIdConfig() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

PreferredAudioOutputDeviceManagerImpl::PreferredAudioOutputDeviceManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

PreferredAudioOutputDeviceManagerImpl::
    ~PreferredAudioOutputDeviceManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void PreferredAudioOutputDeviceManagerImpl::SetPreferredSinkId(
    const GlobalRenderFrameHostToken& main_frame_token,
    const std::string& raw_device_id,
    SetPreferredSinkIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Assign the desired sink ID for the frame. This will trigger the
  // SwitchAudioOutputDeviceId method of any registered DeviceSwitchers, which
  // will update the audio output device. The
  // PreferredAudioOutputDeviceManagerImpl also retains the raw device ID
  // alongside its MainFramePreferredSinkIdConfig object for future reference.
  // When a new audio output device is opened for the default audio output, it
  // will utilize this stored raw device ID if its main frame's
  // MainFramePreferredSinkIdConfig corresponds with the saved
  // MainFramePreferredSinkIdConfig.

  MainFramePreferredSinkIdConfig* config = FindSinkIdConfig(main_frame_token);
  if (config) {
    config->SetPreferredSinkId(raw_device_id);
  } else {
    AddNewConfigEntry(main_frame_token, raw_device_id, nullptr);
  }

  std::move(callback).Run(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
}

void PreferredAudioOutputDeviceManagerImpl::AddSwitcher(
    const GlobalRenderFrameHostToken& main_frame_token,
    AudioOutputDeviceSwitcher* device_switcher) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(device_switcher);

  // The main frame is a key.
  MainFramePreferredSinkIdConfig* config = FindSinkIdConfig(main_frame_token);
  if (config) {
    config->AddDeviceSwitcher(device_switcher);
  } else {
    // The default device is already set.
    AddNewConfigEntry(main_frame_token,
                      media::AudioDeviceDescription::kDefaultDeviceId,
                      device_switcher);
  }
}

void PreferredAudioOutputDeviceManagerImpl::RemoveSwitcher(
    const GlobalRenderFrameHostToken& main_frame_token,
    AudioOutputDeviceSwitcher* device_switcher) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(device_switcher);

  MainFramePreferredSinkIdConfig* config = FindSinkIdConfig(main_frame_token);
  if (!config) {
    // If the page refresh is done before the device switcher removal (race),
    // then it would have no entry.
    return;
  }

  config->RemoveDeviceSwitcher(device_switcher);
}

const std::string& PreferredAudioOutputDeviceManagerImpl::GetPreferredSinkId(
    const GlobalRenderFrameHostToken& main_frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The main frame is a key.
  MainFramePreferredSinkIdConfig* config = FindSinkIdConfig(main_frame_token);
  if (config) {
    return config->preferred_sink_id();
  }

  // default output device.
  static const std::string empty_id;
  return empty_id;
}

void PreferredAudioOutputDeviceManagerImpl::UnregisterMainFrameOnUIThread(
    RenderFrameHost* main_frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (main_frame->GetMainFrame() != main_frame) {
    // Only the main frame can call the API.
    return;
  }

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PreferredAudioOutputDeviceManagerImpl::UnregisterMainFrameOnIOThread,
          weak_ptr_factory_.GetWeakPtr(), main_frame->GetGlobalFrameToken()));
}

void PreferredAudioOutputDeviceManagerImpl::UnregisterMainFrameOnIOThread(
    const GlobalRenderFrameHostToken& main_frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::erase_if(
      preferred_sink_id_configs_,
      [main_frame_token](
          const std::unique_ptr<MainFramePreferredSinkIdConfig>& item) {
        return item->main_frame_global_id() == main_frame_token;
      });
}

void PreferredAudioOutputDeviceManagerImpl::AddNewConfigEntry(
    const GlobalRenderFrameHostToken& main_frame_token,
    const std::string& sink_id,
    AudioOutputDeviceSwitcher* device_switcher) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto config = std::make_unique<MainFramePreferredSinkIdConfig>(
      main_frame_token, sink_id);
  if (device_switcher) {
    config->AddDeviceSwitcher(device_switcher);
  }
  preferred_sink_id_configs_.push_back(std::move(config));
}

PreferredAudioOutputDeviceManagerImpl::MainFramePreferredSinkIdConfig*
PreferredAudioOutputDeviceManagerImpl::FindSinkIdConfig(
    const GlobalRenderFrameHostToken& main_frame_token) const {
  for (const auto& item : preferred_sink_id_configs_) {
    if (item->main_frame_global_id() == main_frame_token) {
      return item.get();
    }
  }

  return nullptr;
}

}  // namespace content
