// Copyright 2016 The Chromium Authors
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

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/system/system_monitor.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_util.h"
#include "content/common/content_export.h"
#include "media/audio/audio_device_description.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"

using blink::mojom::AudioInputDeviceCapabilitiesPtr;
using blink::mojom::MediaDeviceType;
using blink::mojom::VideoInputDeviceCapabilitiesPtr;

namespace media {
class AudioSystem;
}

namespace content {

class MediaDevicesPermissionChecker;
class VideoCaptureManager;

// Use MediaDeviceType values to index on this type.
using MediaDeviceEnumeration =
    std::array<blink::WebMediaDeviceInfoArray,
               static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes)>;

// MediaDevicesManager is responsible for doing media-device enumerations.
// In addition it implements caching for enumeration results and device
// monitoring in order to keep caches consistent.
// All its methods must be called on the IO thread.
class CONTENT_EXPORT MediaDevicesManager
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  // Use MediaDeviceType values to index on this type. By default
  // all device types are false.
  class BoolDeviceTypes final
      : public std::array<bool,
                          static_cast<size_t>(
                              MediaDeviceType::kNumMediaDeviceTypes)> {
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
      MediaDeviceType type,
      const blink::WebMediaDeviceInfo& media_device_info)>;
  using UIInputDeviceChangeCallback = base::RepeatingCallback<void(
      MediaDeviceType stream_type,
      const blink::WebMediaDeviceInfoArray& devices)>;

  MediaDevicesManager(
      media::AudioSystem* audio_system,
      const scoped_refptr<VideoCaptureManager>& video_capture_manager,
      StopRemovedInputDeviceCallback stop_removed_input_device_cb,
      UIInputDeviceChangeCallback ui_input_device_change_cb);

  MediaDevicesManager(const MediaDevicesManager&) = delete;
  MediaDevicesManager& operator=(const MediaDevicesManager&) = delete;

  ~MediaDevicesManager() override;

  // Performs a possibly cached device enumeration for the requested device
  // types and reports the results to `callback`.
  // The enumeration results passed to `callback` are guaranteed to be valid
  // only for the types specified in |requested_types|.
  // Note that this function is not reentrant, so if `callback` needs to perform
  // another call to EnumerateDevices, it must do so by posting a task to the
  // IO thread.
  void EnumerateDevices(const BoolDeviceTypes& requested_types,
                        EnumerationCallback callback);

  // Performs a possibly cached device enumeration for the requested device
  // types and reports the results to `callback`.
  // The enumeration results passed to `callback` are guaranteed to be valid
  // only for the types specified in `requested_types`.
  // Note that this function is not reentrant, so if `callback` needs to perform
  // another call to EnumerateDevices, it must do so by posting a task to the
  // IO thread. The devices will be ordered to match user preference.
  void EnumerateAndRankDevices(GlobalRenderFrameHostId render_frame_host_id,
                               const BoolDeviceTypes& requested_types,
                               EnumerationCallback callback);

  // Performs a possibly cached device enumeration for the requested device
  // types and reports the results to `callback`. The enumeration results are
  // translated for use by the renderer process and frame identified with
  // `render_frame_host_id`, based on the frame origin's
  // permissions, an internal media-device salts.
  // If `request_video_input_capabilities` is true, video formats supported
  // by each device are returned in `callback`. These video formats are in
  // no particular order and may contain duplicate entries. The devices will be
  // ordered to match user preference.
  void EnumerateAndRankDevices(GlobalRenderFrameHostId render_frame_host_id,
                               const BoolDeviceTypes& requested_types,
                               bool request_video_input_capabilities,
                               bool request_audio_input_capabilities,
                               EnumerateDevicesCallback callback);

  uint32_t SubscribeDeviceChangeNotifications(
      GlobalRenderFrameHostId render_frame_host_id,
      const BoolDeviceTypes& subscribe_types,
      mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener);
  void UnsubscribeDeviceChangeNotifications(uint32_t subscription_id);

  // Tries to start device monitoring. If successful, enables caching of
  // enumeration results for the device types supported by the monitor.
  void StartMonitoring();

  // Stops device monitoring and disables caching for all device types.
  void StopMonitoring();

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
      MediaDeviceType type) const;

  const GetMediaDeviceSaltAndOriginCallback& get_salt_and_origin_cb() const {
    return get_salt_and_origin_cb_;
  }

  void RegisterDispatcherHost(
      std::unique_ptr<blink::mojom::MediaDevicesDispatcherHost> dispatcher_host,
      mojo::PendingReceiver<blink::mojom::MediaDevicesDispatcherHost> receiver);
  size_t num_registered_dispatcher_hosts() const {
    return dispatcher_hosts_.size();
  }

  // Used for testing only.
  void SetPermissionChecker(
      std::unique_ptr<MediaDevicesPermissionChecker> permission_checker);
  void set_get_salt_and_origin_cb_for_testing(
      GetMediaDeviceSaltAndOriginCallback callback) {
    get_salt_and_origin_cb_ = std::move(callback);
  }

  // Implementation of video_capture::mojom::DevicesChangedObserver that
  // forwards a devices changed event to the global (process-local) instance of
  // base::DeviceMonitor.
  // Defined in a separate file video_capture_devices_changed_observer.cc
  class VideoCaptureDevicesChangedObserver
      : public video_capture::mojom::DevicesChangedObserver {
    friend class MockVideoCaptureDevicesChangedObserver;

   public:
    explicit VideoCaptureDevicesChangedObserver(
        base::RepeatingClosure disconnect_cb,
        base::RepeatingClosure listener_cb);
    ~VideoCaptureDevicesChangedObserver() override;

    void ConnectToService();

   private:
    // video_capture::mojom::DevicesChangedObserver implementation:
    void OnDevicesChanged() override;

    void OnConnectionError();

    // |disconnect_cb_| is a callback used to invalidate the cache and do a
    // fresh enumeration to avoid losing out on the changes that might happen
    // when the video capture service is not active.
    const base::RepeatingClosure disconnect_cb_;
    const base::RepeatingClosure listener_cb_;
    mojo::Receiver<video_capture::mojom::DevicesChangedObserver> receiver_{
        this};
    mojo::Remote<video_capture::mojom::VideoSourceProvider>
        mojo_device_notifier_;
  };

 private:
  friend class MediaDevicesManagerTest;
  struct EnumerationRequest;

  struct SubscriptionRequest {
    SubscriptionRequest(
        GlobalRenderFrameHostId render_frame_host_id,
        const BoolDeviceTypes& subscribe_types,
        mojo::Remote<blink::mojom::MediaDevicesListener> listener);
    SubscriptionRequest(SubscriptionRequest&&);
    ~SubscriptionRequest();

    SubscriptionRequest& operator=(SubscriptionRequest&&);

    GlobalRenderFrameHostId render_frame_host_id;
    BoolDeviceTypes subscribe_types;
    mojo::Remote<blink::mojom::MediaDevicesListener> listener_;

    // The previously seen device ID salt for this subscription, to be used only
    // to tell if a new salt has been generated, meaning the subscription should
    // be notified that device IDs have changed.
    std::optional<std::string> last_seen_device_id_salt_;
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
  void SetCachePolicy(MediaDeviceType type, CachePolicy policy);

  // Helpers to handle enumeration results for a renderer process.
  void CheckPermissionsForEnumerateDevices(
      GlobalRenderFrameHostId render_frame_host_id,
      const BoolDeviceTypes& requested_types,
      bool request_video_input_capabilities,
      bool request_audio_input_capabilities,
      EnumerateDevicesCallback callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin);
  void OnPermissionsCheckDone(
      GlobalRenderFrameHostId render_frame_host_id,
      const MediaDevicesManager::BoolDeviceTypes& requested_types,
      bool request_video_input_capabilities,
      bool request_audio_input_capabilities,
      EnumerateDevicesCallback callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin,
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
      const std::optional<media::AudioParameters>& parameters);
  void FinalizeDevicesEnumerated(EnumerationState enumeration_state);

  std::vector<VideoInputDeviceCapabilitiesPtr> ComputeVideoInputCapabilities(
      const blink::WebMediaDeviceInfoArray& raw_device_infos,
      const blink::WebMediaDeviceInfoArray& translated_device_infos);

  // Helpers to issue low-level device enumerations.
  void DoEnumerateDevices(MediaDeviceType type);
  void EnumerateAudioDevices(bool is_input);

  // Callback for VideoCaptureManager::EnumerateDevices.
  void VideoInputDevicesEnumerated(
      media::mojom::DeviceEnumerationResult result_code,
      const media::VideoCaptureDeviceDescriptors& descriptors);

  // Callback for AudioSystem::GetDeviceDescriptions.
  void AudioDevicesEnumerated(
      MediaDeviceType type,
      media::AudioDeviceDescriptions device_descriptions);

  // Helpers to handle enumeration results.
  void DevicesEnumerated(MediaDeviceType type,
                         const blink::WebMediaDeviceInfoArray& snapshot);
  void UpdateSnapshot(MediaDeviceType type,
                      const blink::WebMediaDeviceInfoArray& new_snapshot,
                      bool ignore_group_id = true);
  void ProcessRequests();
  bool IsEnumerationRequestReady(const EnumerationRequest& request_info);

  // Helpers to handle device-change notification.
  void HandleDevicesChanged(MediaDeviceType type);
  void MaybeStopRemovedInputDevices(
      MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& new_snapshot);
  void SetSubscriptionLastSeenDeviceIdSalt(
      uint32_t subscription_id,
      const MediaDeviceSaltAndOrigin& salt_and_origin);
  void OnSaltAndOriginForSubscription(
      uint32_t subscription_id,
      GlobalRenderFrameHostId render_frame_host_id,
      MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& device_infos,
      bool devices_changed,
      const MediaDeviceSaltAndOrigin& salt_and_origin);
  void CheckPermissionForDeviceChange(
      uint32_t subscription_id,
      GlobalRenderFrameHostId render_frame_host_id,
      MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& device_infos,
      const MediaDeviceSaltAndOrigin& salt_and_origin);
  void OnCheckedPermissionForDeviceChange(
      uint32_t subscription_id,
      GlobalRenderFrameHostId render_frame_host_id,
      MediaDeviceType type,
      const blink::WebMediaDeviceInfoArray& device_infos,
      const MediaDeviceSaltAndOrigin& salt_and_origin,
      bool has_permission);
  void NotifyDeviceChange(uint32_t subscription_id,
                          MediaDeviceType type,
                          const MediaDeviceSaltAndOrigin& salt_and_origin,
                          bool has_permission,
                          const MediaDeviceEnumeration& enumeration);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  void RegisterVideoCaptureDevicesChangedObserver();
#endif

  bool use_fake_devices_;
  const raw_ptr<media::AudioSystem, DanglingUntriaged>
      audio_system_;  // not owned
  scoped_refptr<VideoCaptureManager> video_capture_manager_;
  StopRemovedInputDeviceCallback stop_removed_input_device_cb_;
  UIInputDeviceChangeCallback ui_input_device_change_cb_;

  std::unique_ptr<MediaDevicesPermissionChecker> permission_checker_;

  using CachePolicies =
      std::array<CachePolicy,
                 static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes)>;
  CachePolicies cache_policies_;

  class CacheInfo;
  using CacheInfos = std::vector<CacheInfo>;
  CacheInfos cache_infos_;

  BoolDeviceTypes cache_is_populated_;
  std::vector<EnumerationRequest> requests_;
  MediaDeviceEnumeration current_snapshot_;
  bool monitoring_started_;

  uint32_t last_subscription_id_ = 0u;
  base::flat_map<uint32_t, SubscriptionRequest> subscriptions_;

  // Callback used to obtain the current device ID salt and security origin.
  GetMediaDeviceSaltAndOriginCallback get_salt_and_origin_cb_;

  class AudioServiceDeviceListener;
  std::unique_ptr<AudioServiceDeviceListener> audio_service_device_listener_;
  std::unique_ptr<VideoCaptureDevicesChangedObserver>
      video_capture_service_device_changed_observer_;

  std::map<uint32_t, EnumerationState> enumeration_states_;
  uint32_t next_enumeration_state_id_ = 0;

  mojo::UniqueReceiverSet<blink::mojom::MediaDevicesDispatcherHost>
      dispatcher_hosts_;

  base::WeakPtrFactory<MediaDevicesManager> weak_factory_{this};
};

// This function uses a heuristic to guess the group ID for a video device with
// label |video_label| based on appearance of |video_label| as a substring in
// the label of any of the audio devices in |audio_infos|. The heuristic tries
// to minimize the probability of false positives.
// If the heuristic fails to find an association, the |video_info.device_id| is
// returned to be used as group ID. This group ID and the device ID are later
// obfuscated with different salts before being sent to the renderer process.
// TODO(crbug.com/41263713): Replace the heuristic with proper associations
// provided by the OS.
CONTENT_EXPORT std::string GuessVideoGroupID(
    const blink::WebMediaDeviceInfoArray& audio_infos,
    const blink::WebMediaDeviceInfo& video_info);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_MANAGER_H_
