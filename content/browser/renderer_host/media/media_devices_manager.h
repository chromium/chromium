// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_MANAGER_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/system/system_monitor.h"
#include "content/browser/media/media_devices_util.h"
#include "content/common/content_export.h"
#include "media/audio/audio_device_description.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"

using blink::mojom::AudioInputDeviceCapabilitiesPtr;
using blink::mojom::VideoInputDeviceCapabilitiesPtr;

namespace media {
class AudioSystem;
}

namespace service_manager {
class Connector;
}

namespace content {

class MediaDevicesPermissionChecker;
class VideoCaptureManager;

// Use blink::MediaDeviceType values to index on this type.
using MediaDeviceEnumeration =
    std::array<blink::WebMediaDeviceInfoArray, blink::NUM_MEDIA_DEVICE_TYPES>;

// MediaDevicesManager is responsible for doing media-device enumerations.
// In addition it implements caching for enumeration results and device
// monitoring in order to keep caches consistent.
// All its methods must be called on the IO thread.
class CONTENT_EXPORT MediaDevicesManager
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  // Use blink::MediaDeviceType values to index on this type. By default all
  // device types are false.
  class BoolDeviceTypes final
      : public std::array<bool, blink::NUM_MEDIA_DEVICE_TYPES> {
   public:
    BoolDeviceTypes() { fill(false); }
  };

  using EnumerationCallback =
      base::OnceCallback<void(const MediaDeviceEnumeration&)>;
  using EnumerateDevicesCallback = base::OnceCallback<void(
      const std::vector<blink::WebMediaDeviceInfoArray>&,
      std::vector<VideoInputDeviceCapabilitiesPtr>,
      std::vector<AudioInputDeviceCapabilitiesPtr>)>;
  using StopRemovedInputDeviceCallback = base::RepeatingCallback<void(
      blink::MediaDeviceType type,
      const blink::WebMediaDeviceInfo& media_device_info)>;
  using UIInputDeviceChangeCallback = base::RepeatingCallback<void(
      blink::MediaDeviceType stream_type,
      const blink::WebMediaDeviceInfoArray& devices)>;

  MediaDevicesManager(
      media::AudioSystem* audio_system,
      const scoped_refptr<VideoCaptureManager>& video_capture_manager,
      StopRemovedInputDeviceCallback stop_removed_input_device_cb,
      UIInputDeviceChangeCallback ui_input_device_change_cb);
  ~MediaDevicesManager() override;

  // Performs a possibly cached device enumeration for the requested device
  // types and reports the results to |callback|.
  // The enumeration results passed to |callback| are guaranteed to be valid
  // only for the types specified in |requested_types|.
  // Note that this function is not reentrant, so if |callback| needs to perform
  // another call to EnumerateDevices, it must do so by posting a task to the
  // IO thread.
  void EnumerateDevices(const BoolDeviceTypes& requested_types,
                        EnumerationCallback callback);

  // Performs a possibly cached device enumeration for the requested device
  // types and reports the results to |callback|. The enumeration results are
  // translated for use by the renderer process and frame identified with
  // |render_process_id| and |render_frame_id|, based on the frame origin's
  // permissions, an internal media-device salts.
  // If |request_video_input_capabilities| is true, video formats supported
  // by each device are returned in |callback|. These video formats are in
  // no particular order and may contain duplicate entries.
  void EnumerateDevices(int render_process_id,
                        int render_frame_id,
                        const BoolDeviceTypes& requested_types,
                        bool request_video_input_capabilities,
                        bool request_audio_input_capabilities,
                        EnumerateDevicesCallback callback);

  uint32_t SubscribeDeviceChangeNotifications(
      int render_process_id,
      int render_frame_id,
      const BoolDeviceTypes& subscribe_types,
      mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener);
  void UnsubscribeDeviceChangeNotifications(uint32_t subscription_id);

  // Tries to start device monitoring. If successful, enables caching of
  // enumeration results for the device types supported by the monitor.
  void StartMonitoring();

  // Stops device monitoring and disables caching for all device types.
  void StopMonitoring();

  // Returns true if device monitoring is active, false otherwise.
  bool IsMonitoringStarted();

  // Implements base::SystemMonitor::DevicesChangedObserver.
  // This function is only called in response to physical audio/video device
  // changes.
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

  // Returns the supported video formats for the given |device_id|. The returned
  // formats are in no particular order and may contain duplicate entries.
  // If |try_in_use_first| is true and the device is being used, only the format
  // in use is returned. Otherwise, all formats supported by the device are
  // returned.
  media::VideoCaptureFormats GetVideoInputFormats(const std::string& device_id,
                                                  bool try_in_use_first);

  // TODO(guidou): Remove this function once content::GetMediaDeviceIDForHMAC
  // is rewritten to receive devices via a callback.
  // See http://crbug.com/648155.
  blink::WebMediaDeviceInfoArray GetCachedDeviceInfo(
      blink::MediaDeviceType type);

  MediaDevicesPermissionChecker* media_devices_permission_checker();

  const MediaDeviceSaltAndOriginCallback& salt_and_origin_callback() const {
    return salt_and_origin_callback_;
  }

  // Used for testing only.
  void SetPermissionChecker(
      std::unique_ptr<MediaDevicesPermissionChecker> permission_checker);
  void set_salt_and_origin_callback_for_testing(
      MediaDeviceSaltAndOriginCallback callback) {
    salt_and_origin_callback_ = std::move(callback);
  }

 private:
  friend class MediaDevicesManagerTest;
  struct EnumerationRequest;

  struct SubscriptionRequest {
    SubscriptionRequest(
        int render_process_id,
        int render_frame_id,
        const BoolDeviceTypes& subscribe_types,
        mojo::Remote<blink::mojom::MediaDevicesListener> listener);
    SubscriptionRequest(SubscriptionRequest&&);
    ~SubscriptionRequest();

    SubscriptionRequest& operator=(SubscriptionRequest&&);

    int render_process_id;
    int render_frame_id;
    BoolDeviceTypes subscribe_types;
    mojo::Remote<blink::mojom::MediaDevicesListener> listener_;
  };

  // Class containing the state of each spawned enumeration. This state is
  // required since retrieving audio parameters is done asynchronously for each
  // device and a new devices enumeration request can be started before all
  // parameters have been collected.
  class EnumerationState {
   public:
    EnumerationState();
    EnumerationState(EnumerationState&& other);
    ~EnumerationState();

    EnumerationState& operator=(EnumerationState&& other);

    bool video_input_capabilities_requested = false;
    bool audio_input_capabilities_requested = false;
    EnumerateDevicesCallback completion_cb;
    std::vector<AudioInputDeviceCapabilitiesPtr> audio_capabilities;
    int num_pending_audio_input_capabilities;
    MediaDeviceEnumeration raw_enumeration_results;
    std::vector<blink::WebMediaDeviceInfoArray> hashed_enumeration_results;
  };

  // The NO_CACHE policy is such that no previous results are used when
  // EnumerateDevices is called. The results of a new or in-progress low-level
  // device enumeration are used.
  // The SYSTEM_MONITOR policy is such that previous results are re-used,
  // provided they were produced by a low-level device enumeration issued after
  // the last call to OnDevicesChanged.
  enum class CachePolicy {
    NO_CACHE,
    SYSTEM_MONITOR,
  };

  // Manually sets a caching policy for a given device type.
  void SetCachePolicy(blink::MediaDeviceType type, CachePolicy policy);

  // Helpers to handle enumeration results for a renderer process.
  void CheckPermissionsForEnumerateDevices(
      int render_process_id,
      int render_frame_id,
      const BoolDeviceTypes& requested_types,
      bool request_video_input_capabilities,
      bool request_audio_input_capabilities,
      EnumerateDevicesCallback callback,
      MediaDeviceSaltAndOrigin salt_and_origin);
  void OnPermissionsCheckDone(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      bool request_video_input_capabilities,
      bool request_audio_input_capabilities,
      EnumerateDevicesCallback callback,
      MediaDeviceSaltAndOrigin salt_and_origin,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions);
  void OnDevicesEnumerated(
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      bool request_video_input_capabilities,
      bool request_audio_input_capabilities,
      EnumerateDevicesCallback callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin,
      const MediaDevicesManager::BoolDeviceTypes& has_permissions,
      const MediaDeviceEnumeration& enumeration);
  void GetAudioInputCapabilities(
      bool request_video_input_capabilities,
      bool request_audio_input_capabilities,
      EnumerateDevicesCallback callback,
      const MediaDeviceEnumeration& enumeration,
      const std::vector<blink::WebMediaDeviceInfoArray>& devices_info);
  void GotAudioInputCapabilities(
      size_t state_index,
      size_t capabilities_index,
      const base::Optional<media::AudioParameters>& parameters);
  void FinalizeDevicesEnumerated(EnumerationState enumeration_state);

  std::vector<VideoInputDeviceCapabilitiesPtr> ComputeVideoInputCapabilities(
      const blink::WebMediaDeviceInfoArray& raw_device_infos,
      const blink::WebMediaDeviceInfoArray& translated_device_infos);

  // Helpers to issue low-level device enumerations.
  void DoEnumerateDevices(blink::MediaDeviceType type);
  void EnumerateAudioDevices(bool is_input);

  // Callback for VideoCaptureManager::EnumerateDevices.
  void VideoInputDevicesEnumerated(
      const media::VideoCaptureDeviceDescriptors& descriptors);

  // Callback for AudioSystem::GetDeviceDescriptions.
  void AudioDevicesEnumerated(
      blink::MediaDeviceType type,
      media::AudioDeviceDescriptions device_descriptions);

  // Helpers to handle enumeration results.
  void DevicesEnumerated(blink::MediaDeviceType type,
                         const blink::WebMediaDeviceInfoArray& snapshot);
  void UpdateSnapshot(blink::MediaDeviceType type,
                      const blink::WebMediaDeviceInfoArray& new_snapshot,
                      bool ignore_group_id = true);
  void ProcessRequests();
  bool IsEnumerationRequestReady(const EnumerationRequest& request_info);

  // Helpers to handle device-change notification.
  void HandleDevicesChanged(blink::MediaDeviceType type);
  void MaybeStopRemovedInputDevices(
      blink::MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& new_snapshot);
  void NotifyDeviceChangeSubscribers(
      blink::MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& snapshot);
  void CheckPermissionForDeviceChange(
      uint32_t subscription_id,
      int render_process_id,
      int render_frame_id,
      blink::MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& device_infos,
      MediaDeviceSaltAndOrigin salt_and_origin);
  void NotifyDeviceChange(uint32_t subscription_id,
                          blink::MediaDeviceType type,
                          const blink::WebMediaDeviceInfoArray& device_infos,
                          const MediaDeviceSaltAndOrigin& salt_and_origin,
                          bool has_permission);

#if defined(OS_MACOSX)
  void StartMonitoringOnUIThread();
#endif

  bool use_fake_devices_;
  media::AudioSystem* const audio_system_;  // not owned
  scoped_refptr<VideoCaptureManager> video_capture_manager_;
  StopRemovedInputDeviceCallback stop_removed_input_device_cb_;
  UIInputDeviceChangeCallback ui_input_device_change_cb_;

  std::unique_ptr<MediaDevicesPermissionChecker> permission_checker_;

  using CachePolicies = std::array<CachePolicy, blink::NUM_MEDIA_DEVICE_TYPES>;
  CachePolicies cache_policies_;

  class CacheInfo;
  using CacheInfos = std::vector<CacheInfo>;
  CacheInfos cache_infos_;

  BoolDeviceTypes has_seen_result_;
  std::vector<EnumerationRequest> requests_;
  MediaDeviceEnumeration current_snapshot_;
  bool monitoring_started_;

  uint32_t last_subscription_id_ = 0u;
  base::flat_map<uint32_t, SubscriptionRequest> subscriptions_;

  // Callback used to obtain the current device ID salt and security origin.
  MediaDeviceSaltAndOriginCallback salt_and_origin_callback_;

  std::unique_ptr<service_manager::Connector> connector_;

  class AudioServiceDeviceListener;
  std::unique_ptr<AudioServiceDeviceListener> audio_service_device_listener_;

  std::map<uint32_t, EnumerationState> enumeration_states_;
  uint32_t next_enumeration_state_id_ = 0;

  base::WeakPtrFactory<MediaDevicesManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesManager);
};

// This function uses a heuristic to guess the group ID for a video device with
// label |video_label| based on appearance of |video_label| as a substring in
// the label of any of the audio devices in |audio_infos|. The heuristic tries
// to minimize the probability of false positives.
// If the heuristic fails to find an association, the |video_info.device_id| is
// returned to be used as group ID. This group ID and the device ID are later
// obfuscated with different salts before being sent to the renderer process.
// TODO(crbug.com/627793): Replace the heuristic with proper associations
// provided by the OS.
CONTENT_EXPORT std::string GuessVideoGroupID(
    const blink::WebMediaDeviceInfoArray& audio_infos,
    const blink::WebMediaDeviceInfo& video_info);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_MANAGER_H_
