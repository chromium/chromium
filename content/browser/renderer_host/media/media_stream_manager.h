// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamManager is used to open media capture devices.
//
// If either user or test harness selects --use-fake-device-for-media-stream,
// a fake video device or devices are used instead of real ones.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/capture_handle_manager.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/browser/renderer_host/media/media_stream_power_logger.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/permission_controller.h"
#include "media/base/video_facing.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/browser/media/captured_surface_controller.h"
#endif

namespace media {
class AudioSystem;
#if BUILDFLAG(IS_CHROMEOS_ASH)
class JpegAcceleratorProviderImpl;
class SystemEventMonitorImpl;
#endif
}

namespace url {
class Origin;
}

namespace content {

class AudioInputDeviceManager;
class AudioServiceListener;
class FakeMediaStreamUIProxy;
class MediaStreamUIProxy;
class PermissionControllerImpl;
class VideoCaptureManager;
class VideoCaptureProvider;

enum TransferState { KEPT_ALIVE, GOT_OPEN_DEVICE };

struct TransferStatus {
  TransferState state;
  base::TimeTicks start_time;
};
typedef std::map<const base::UnguessableToken, TransferStatus> TransferMap;

// MediaStreamManager is used to generate and close new media devices, not to
// start the media flow. The classes requesting new media streams are answered
// using callbacks.
class CONTENT_EXPORT MediaStreamManager
    : public MediaStreamProviderListener,
      public base::CurrentThread::DestructionObserver {
 public:
  // Callback to deliver the result of a media access request.
  using MediaAccessRequestCallback = base::OnceCallback<void(
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      std::unique_ptr<MediaStreamUIProxy> ui)>;

  using GenerateStreamsCallback = base::OnceCallback<void(
      blink::mojom::MediaStreamRequestResult result,
      const std::string& label,
      blink::mojom::StreamDevicesSetPtr stream_devices_set,
      bool pan_tilt_zoom_allowed)>;

  using OpenDeviceCallback =
      base::OnceCallback<void(bool success,
                              const std::string& label,
                              const blink::MediaStreamDevice& device)>;

  using DeviceStoppedCallback =
      base::RepeatingCallback<void(const std::string& label,
                                   const blink::MediaStreamDevice& device)>;

  using DeviceChangedCallback =
      base::RepeatingCallback<void(const std::string& label,
                                   const blink::MediaStreamDevice& old_device,
                                   const blink::MediaStreamDevice& new_device)>;

  using DeviceRequestStateChangeCallback = base::RepeatingCallback<void(
      const std::string& label,
      const blink::MediaStreamDevice& device,
      const blink::mojom::MediaStreamStateChange new_state)>;

  using DeviceCaptureConfigurationChangeCallback =
      base::RepeatingCallback<void(const std::string& label,
                                   const blink::MediaStreamDevice& device)>;

  using DeviceCaptureHandleChangeCallback =
      base::RepeatingCallback<void(const std::string& label,
                                   const blink::MediaStreamDevice& device)>;

  using ZoomLevelChangeCallback =
      base::RepeatingCallback<void(const std::string& label,
                                   const blink::MediaStreamDevice& device,
                                   int zoom_level)>;

  using GetOpenDeviceCallback =
      base::OnceCallback<void(blink::mojom::MediaStreamRequestResult result,
                              blink::mojom::GetOpenDeviceResponsePtr response)>;

  // Callback for testing.
  using GenerateStreamTestCallback =
      base::OnceCallback<bool(const blink::StreamControls&)>;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Callback for creating a CapturedSurfaceController. Used to override the
  // default CapturedSurfaceController in tests.
  using CapturedSurfaceControllerFactoryCallback =
      ::base::RepeatingCallback<std::unique_ptr<CapturedSurfaceController>(
          GlobalRenderFrameHostId,
          WebContentsMediaCaptureId,
          base::RepeatingCallback<void(int)> on_zoom_level_change_callback)>;
#endif

  // Adds |message| to native logs for outstanding device requests, for use by
  // render processes hosts whose corresponding render processes are requesting
  // logging from webrtcLoggingPrivate API. Safe to call from any thread.
  static void SendMessageToNativeLog(const std::string& message);

  // Returns the current MediaStreamManager instance. This is used to get access
  // to the getters that return objects owned by MediaStreamManager.
  // Must be called on the IO thread.
  static MediaStreamManager* GetInstance();

  explicit MediaStreamManager(media::AudioSystem* audio_system);

  // |audio_system| is required but defaults will be used if either
  // |video_capture_system| or |device_task_runner| are null.
  MediaStreamManager(
      media::AudioSystem* audio_system,
      std::unique_ptr<VideoCaptureProvider> video_capture_provider);

  MediaStreamManager(const MediaStreamManager&) = delete;
  MediaStreamManager& operator=(const MediaStreamManager&) = delete;

  ~MediaStreamManager() override;

  // Used to access VideoCaptureManager.
  VideoCaptureManager* video_capture_manager() const;

  // Used to access AudioInputDeviceManager.
  AudioInputDeviceManager* audio_input_device_manager() const;

  // Used to access AudioServiceListener, must be called on UI thread.
  AudioServiceListener* audio_service_listener();

  // Used to access MediaDevicesManager.
  MediaDevicesManager* media_devices_manager();

  // Used to access AudioSystem.
  media::AudioSystem* audio_system();

  // AddVideoCaptureObserver() and RemoveAllVideoCaptureObservers() must be
  // called after InitializeDeviceManagersOnIOThread() and before
  // WillDestroyCurrentMessageLoop(). They can be called more than once and it's
  // ok to not call at all if the client is not interested in receiving
  // media::VideoCaptureObserver callbacks.
  // The methods must be called on BrowserThread::IO threads. The callbacks of
  // media::VideoCaptureObserver also arrive on BrowserThread::IO threads.
  void AddVideoCaptureObserver(media::VideoCaptureObserver* capture_observer);
  void RemoveAllVideoCaptureObservers();

  // Creates a new media access request which is identified by a unique string
  // that's returned to the caller. This will trigger the infobar and ask users
  // for access to the device. `render_frame_host_id` is used to determine where
  // the infobar will appear to the user. |callback| is used to send the
  // selected device to the clients. An empty list of device will be returned if
  // the users deny the access.
  std::string MakeMediaAccessRequest(
      GlobalRenderFrameHostId render_frame_host_id,
      int requester_id,
      int page_request_id,
      const blink::StreamControls& controls,
      const url::Origin& security_origin,
      MediaAccessRequestCallback callback);

  // GenerateStream opens new media devices according to |controls|. It creates
  // a new request which is identified by a unique string that's returned to the
  // caller. `render_frame_host_id` is used to determine where the infobar will
  // appear to the user. |device_stopped_callback| is set to receive device
  // stopped notifications. |device_changed_callback| is set to receive device
  // changed notifications.  |device_request_state_change_callback| is used to
  // notify clients about request state changes.  TODO(crbug.com/40058526):
  // Package device-related callbacks into a single struct.
  void GenerateStreams(
      GlobalRenderFrameHostId render_frame_host_id,
      int requester_id,
      int page_request_id,
      const blink::StreamControls& controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamsCallback generate_stream_callback,
      DeviceStoppedCallback device_stopped_callback,
      DeviceChangedCallback device_changed_callback,
      DeviceRequestStateChangeCallback device_request_state_change_callback,
      DeviceCaptureConfigurationChangeCallback
          device_capture_configuration_change_callback,
      DeviceCaptureHandleChangeCallback device_capture_handle_change_callback,
      ZoomLevelChangeCallback zoom_level_change_callback);

  // Accesses an existing open device, identified by |device_session_id|,
  // and associates it with a new DeviceRequest. This device is then returned by
  // invoking |get_open_device_callback| asynchronously.
  void GetOpenDevice(
      const base::UnguessableToken& device_session_id,
      const base::UnguessableToken& transfer_id,
      GlobalRenderFrameHostId requesting_render_frame_host_id,
      int requester_id,
      int page_request_id,
      MediaDeviceSaltAndOrigin salt_and_origin,
      GetOpenDeviceCallback get_open_device_callback,
      DeviceStoppedCallback device_stopped_callback,
      DeviceChangedCallback device_changed_callback,
      DeviceRequestStateChangeCallback device_request_state_change_callback,
      DeviceCaptureConfigurationChangeCallback
          device_capture_configuration_change_callback,
      DeviceCaptureHandleChangeCallback device_capture_handle_change_callback,
      ZoomLevelChangeCallback zoom_level_change_callback);

  // Cancel an open request identified by |page_request_id| for the given frame.
  // Must be called on the IO thread.
  void CancelRequest(GlobalRenderFrameHostId render_frame_host_id,
                     int requester_id,
                     int page_request_id);

  // Cancel an open request identified by |label|. Must be called on the IO
  // thread.
  void CancelRequest(const std::string& label);

  // Cancel all requests for the given `render_frame_host_id`. Must be called on
  // the IO thread.
  void CancelAllRequests(GlobalRenderFrameHostId render_frame_host_id,
                         int requester_id);

  // Closes the stream device for a certain render frame. The stream must have
  // been opened by a call to GenerateStream or GetOpenDevice. Must be called on
  // the IO thread. Closing a cloned MediaStreamDevice, created by
  // GetOpenDevice, doesn't affect the original MediaStreamDevice it was created
  // from. Similarly, closing the original MediaStreamDevice, created by
  // GenerateStream, doesn't affect the cloned device.
  void StopStreamDevice(GlobalRenderFrameHostId render_frame_host_id,
                        int requester_id,
                        const std::string& device_id,
                        const base::UnguessableToken& session_id);

  // Open a device identified by |device_id|. |type| must be either
  // MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.
  // |device_stopped_callback| is set to receive device stopped notifications.
  // The request is identified using string returned to the caller.
  void OpenDevice(GlobalRenderFrameHostId render_frame_host_id,
                  int requester_id,
                  int page_request_id,
                  const std::string& device_id,
                  blink::mojom::MediaStreamType type,
                  MediaDeviceSaltAndOrigin salt_and_origin,
                  OpenDeviceCallback open_device_callback,
                  DeviceStoppedCallback device_stopped_callback);

  // Find |device_id| in the list of |requests_|, and returns its session id,
  // or an empty base::UnguessableToken if not found. Must be called on the IO
  // thread.
  base::UnguessableToken VideoDeviceIdToSessionId(
      const std::string& device_id) const;

  // Called by UI to make sure the device monitor is started so that UI receive
  // notifications about device changes.
  void EnsureDeviceMonitorStarted();

  // Implements MediaStreamProviderListener.
  void Opened(blink::mojom::MediaStreamType stream_type,
              const base::UnguessableToken& capture_session_id) override;
  void Closed(blink::mojom::MediaStreamType stream_type,
              const base::UnguessableToken& capture_session_id) override;
  void Aborted(blink::mojom::MediaStreamType stream_type,
               const base::UnguessableToken& capture_session_id) override;

  // Returns all devices currently opened by a request with label |label|.
  // If no request with |label| exist, an empty array is returned.
  blink::MediaStreamDevices GetDevicesOpenedByRequest(
      const std::string& label) const;

  using GetRawDeviceIdsOpenedForFrameCallback =
      base::OnceCallback<void(std::vector<std::string> active_device_ids)>;
  // Returns all device IDs currently opened for `render_frame_host_id` and its
  // descendants with `type`. If no request exists that matches the constraints,
  // an empty array is returned.
  void GetRawDeviceIdsOpenedForFrame(
      RenderFrameHost* render_frame_host,
      blink::mojom::MediaStreamType type,
      GetRawDeviceIdsOpenedForFrameCallback) const;

  // This object gets deleted on the UI thread after the IO thread has been
  // destroyed. So we need to know when IO thread is being destroyed so that
  // we can delete VideoCaptureManager and AudioInputDeviceManager.
  // Note: In tests it is sometimes necessary to invoke this explicitly when
  // using BrowserTaskEnvironment because the notification happens too late.
  // (see http://crbug.com/247525#c14).
  void WillDestroyCurrentMessageLoop() override;

  // Sends log messages to the render process hosts whose corresponding render
  // processes are making device requests, to be used by the
  // webrtcLoggingPrivate API if requested.
  void AddLogMessageOnIOThread(const std::string& message);

  // Called by the tests to specify a factory for creating
  // FakeMediaStreamUIProxys to be used for generated streams.
  void UseFakeUIFactoryForTests(
      base::RepeatingCallback<std::unique_ptr<FakeMediaStreamUIProxy>(void)>
          fake_ui_factory,
      bool use_for_gum_desktop_capture = true,
      std::optional<WebContentsMediaCaptureId> captured_tab_id = std::nullopt);

  // Register and unregister a new callback for receiving native log entries.
  // Called on the IO thread.
  static void RegisterNativeLogCallback(
      int renderer_host_id,
      base::RepeatingCallback<void(const std::string&)> callback);
  static void UnregisterNativeLogCallback(int renderer_host_id);

  // Returns true if the renderer process identified with |render_process_id|
  // is allowed to access |origin|.
  static bool IsOriginAllowed(int render_process_id, const url::Origin& origin);

  // Set whether the capturing is secure for the capturing session with given
  // |session_id|, |render_process_id|, and the MediaStreamType |type|.
  // Must be called on the IO thread.
  void SetCapturingLinkSecured(int render_process_id,
                               const base::UnguessableToken& session_id,
                               blink::mojom::MediaStreamType type,
                               bool is_secure);

  // Helper for sending up-to-date device lists to media observer when a
  // capture device is plugged in or unplugged.
  void NotifyDevicesChanged(blink::mojom::MediaDeviceType stream_type,
                            const blink::WebMediaDeviceInfoArray& devices);

  // This method is called when an audio or video device is removed. It makes
  // sure all MediaStreams that use a removed device are stopped and that the
  // render process is notified. Must be called on the IO thread.
  void StopRemovedDevice(blink::mojom::MediaDeviceType type,
                         const blink::WebMediaDeviceInfo& media_device_info);

  void SetGenerateStreamsCallbackForTesting(
      GenerateStreamTestCallback test_callback);

  void SetStateForTesting(size_t request_index,
                          blink::mojom::MediaStreamType stream_type,
                          MediaRequestState new_state);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void SetCapturedSurfaceControllerFactoryForTesting(
      CapturedSurfaceControllerFactoryCallback factory);
#endif

  // This method is called when all tracks are started.
  void OnStreamStarted(const std::string& label);

  // Keeps MediaStreamDevice alive to allow transferred tracks to successfully
  // find and clone it. Returns whether the specified device was found and
  // successfully kept alive.
  bool KeepDeviceAliveForTransfer(GlobalRenderFrameHostId render_frame_host_id,
                                  int requester_id,
                                  const base::UnguessableToken& session_id,
                                  const base::UnguessableToken& transfer_id);

  void OnCaptureConfigurationChanged(const base::UnguessableToken& session_id);

  void OnRegionCaptureRectChanged(
      const base::UnguessableToken& session_id,
      const std::optional<gfx::Rect>& region_capture_rect);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Determines whether the captured surface (tab/window) should be focused.
  // This can be called at most once, and only within the first 1s of the
  // capture session being initiated. If a call with |focus=false| is not
  // executed within this time period, the captured surface *is* focused.
  //
  // |is_from_microtask| and |is_from_timer| are used to distinguish:
  // a. Explicit calls from the Web-application.
  // b. Implicit calls resulting from the focusability-window-closing microtask.
  // c. The browser-side timer.
  // This distinction is reflected by UMA.
  void SetCapturedDisplaySurfaceFocus(const std::string& label,
                                      bool focus,
                                      bool is_from_microtask,
                                      bool is_from_timer);

  // Captured Surface Control APIs.
  void SendWheel(
      GlobalRenderFrameHostId capturer_rfh_id,
      const base::UnguessableToken& session_id,
      blink::mojom::CapturedWheelActionPtr action,
      base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
          callback);

  void SetZoomLevel(
      GlobalRenderFrameHostId capturer_rfh_id,
      const base::UnguessableToken& session_id,
      int zoom_level,
      base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
          callback);

  void RequestCapturedSurfaceControlPermission(
      GlobalRenderFrameHostId capturer_rfh_id,
      const base::UnguessableToken& session_id,
      base::OnceCallback<void(blink::mojom::CapturedSurfaceControlResult)>
          callback);

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  void RegisterDispatcherHost(
      std::unique_ptr<blink::mojom::MediaStreamDispatcherHost> host,
      mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost> receiver);
  size_t num_dispatcher_hosts() const { return dispatcher_hosts_.size(); }

  void RegisterVideoCaptureHost(
      std::unique_ptr<media::mojom::VideoCaptureHost> host,
      mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver);
  size_t num_video_capture_hosts() const { return video_capture_hosts_.size(); }

  std::optional<url::Origin> GetOriginByVideoSessionId(
      const base::UnguessableToken& session_id);

 private:
  friend class MediaStreamManagerTest;
  FRIEND_TEST_ALL_PREFIXES(MediaStreamManagerTest, DesktopCaptureDeviceStopped);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamManagerTest, DesktopCaptureDeviceChanged);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamManagerTest,
                           MultiCaptureOnMediaStreamUIWindowId);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamManagerTest,
                           MultiCaptureAllDevicesOpened);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamManagerTest,
                           MultiCaptureNotAllDevicesOpened);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamManagerTest,
                           MultiCaptureIntermediateErrorOnOpening);

  // Contains common data needed to keep track of requests.
  class DeviceRequest;
  // Contains data specific for MediaDeviceAccess requests
  class MediaAccessRequest;
  // Contains common data between GenerateStreams and GetOpenDevice requests
  class CreateDeviceRequest;
  // Contains data specific for GenerateStreams requests
  class GenerateStreamsRequest;
  // Contains data specific for GetOpenDevice requests
  class GetOpenDeviceRequest;
  // Contains data specific for OpenDevice requests
  class OpenDeviceRequest;

  // |DeviceRequests| is a list to ensure requests are processed in the order
  // they arrive. The first member of the pair is the label of the
  // |DeviceRequest|.
  using LabeledDeviceRequest =
      std::pair<std::string, std::unique_ptr<DeviceRequest>>;
  using DeviceRequests = std::list<LabeledDeviceRequest>;

  // Sets the |device| to the right audio / video field in |target_devices|.
  static void SetRequestDevice(blink::mojom::StreamDevices& target_devices,
                               const blink::MediaStreamDevice& device);

  void InitializeMaybeAsync(
      std::unique_ptr<VideoCaptureProvider> video_capture_provider);

  // |output_parameters| contains real values only if the request requires it.
  void HandleAccessRequestResponse(
      const std::string& label,
      const media::AudioParameters& output_parameters,
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result);
  void HandleChangeSourceRequestResponse(
      const std::string& label,
      DeviceRequest* request,
      const blink::mojom::StreamDevicesSet& stream_devices_set);
  void StopMediaStreamFromBrowser(const std::string& label);
  void ChangeMediaStreamSourceFromBrowser(const std::string& label,
                                          const DesktopMediaID& media_id,
                                          bool captured_surface_control_active);
  void OnRequestStateChangeFromBrowser(
      const std::string& label,
      const DesktopMediaID& media_id,
      blink::mojom::MediaStreamStateChange new_state);

  // Checks if all devices that was requested in the request identififed by
  // |label| has been opened and set the request state accordingly.
  void HandleRequestDone(const std::string& label, DeviceRequest* request);

  // Stop the use of the device associated with |session_id| of type |type| in
  // all |requests_|. The device is removed from the request. If a request
  // doesn't use any devices as a consequence, the request is deleted.
  void StopDevice(blink::mojom::MediaStreamType type,
                  const base::UnguessableToken& session_id);

  // Calls the correct capture manager and closes the device with |session_id|.
  // All requests that use the device are updated.
  void CloseDevice(blink::mojom::MediaStreamType type,
                   const base::UnguessableToken& session_id);

  // Returns true if a request for devices has been completed and the devices
  // has either been opened or an error has occurred.
  bool RequestDone(const DeviceRequest& request) const;

  MediaStreamProvider* GetDeviceManager(
      blink::mojom::MediaStreamType stream_type) const;

  void StartEnumeration(DeviceRequest* request, const std::string& label);

  // Adds a new request. Returns an iterator to that request, as stored
  // in requests_.
  DeviceRequests::const_iterator AddRequest(
      std::unique_ptr<DeviceRequest> request);

  DeviceRequests::const_iterator FindRequestIterator(
      const std::string& label) const;
  DeviceRequest* FindRequest(const std::string& label) const;

  // Find a request by the session-ID of its video device.
  // (In case of multiple video devices - any of them would fit.)
  // TOOD(crbug.com/1466247): Remove this after making the Captured Surface
  // Control APIs pass the label instead.
  DeviceRequest* FindRequestByVideoSessionId(
      const base::UnguessableToken& session_id) const;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  CapturedSurfaceController* GetCapturedSurfaceController(
      GlobalRenderFrameHostId capturer_rfh_id,
      const base::UnguessableToken& session_id,
      blink::mojom::CapturedSurfaceControlResult& result);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Clones an existing device identified by |existing_device_session_id| and
  // returns it. If no such device is found, it returns std::nullopt.
  std::optional<blink::MediaStreamDevice> CloneExistingOpenDevice(
      const base::UnguessableToken& existing_device_session_id,
      const base::UnguessableToken& transfer_id,
      const std::string& new_label);

  void UpdateDeviceTransferStatus(DeviceRequest* request,
                                  const blink::MediaStreamDevice* const device,
                                  const base::UnguessableToken& transfer_id,
                                  TransferState transfer_state);
  void CancelRequest(DeviceRequests::const_iterator request_it);
  void DeleteRequest(DeviceRequests::const_iterator request_it);

  // Prepare the request with label |label| by starting device enumeration if
  // needed.
  void SetUpRequest(const std::string& label);

  // Prepare |request| of type MEDIA_DEVICE_AUDIO_CAPTURE and/or
  // MEDIA_DEVICE_VIDEO_CAPTURE for being posted to the UI by parsing
  // StreamControls for requested device IDs.
  bool SetUpDeviceCaptureRequest(DeviceRequest* request,
                                 const MediaDeviceEnumeration& enumeration);

  // Prepare |request| of type MEDIA_DISPLAY_CAPTURE.
  bool SetUpDisplayCaptureRequest(DeviceRequest* request);

  // Prepare |request| of type MEDIA_GUM_DESKTOP_AUDIO_CAPTURE and/or
  // MEDIA_GUM_DESKTOP_VIDEO_CAPTURE for being posted to the UI by parsing
  // StreamControls for the requested desktop ID.
  bool SetUpScreenCaptureRequest(DeviceRequest* request);

  // Resolve the random device ID of tab capture on UI thread before proceeding
  // with the tab capture UI request.
  bool SetUpTabCaptureRequest(DeviceRequest* request, const std::string& label);

  // Prepare |request| for being posted to the UI to bring up the picker again
  // to change the desktop capture source.
  void SetUpDesktopCaptureChangeSourceRequest(DeviceRequest* request,
                                              const std::string& label,
                                              const DesktopMediaID& media_id);

  DesktopMediaID ResolveTabCaptureDeviceIdOnUIThread(
      const std::string& capture_device_id,
      GlobalRenderFrameHostId render_frame_host_id,
      const GURL& origin);

  // Prepare |request| of type MEDIA_GUM_TAB_AUDIO_CAPTURE and/or
  // MEDIA_GUM_TAB_VIDEO_CAPTURE for being posted to the UI after the
  // requested tab capture IDs are resolved from registry.
  void FinishTabCaptureRequestSetupWithDeviceId(
      const std::string& label,
      const DesktopMediaID& device_id);

  // Called when a request has been setup and devices have been enumerated if
  // needed.
  void ReadOutputParamsAndPostRequestToUI(
      const std::string& label,
      DeviceRequest* request,
      const MediaDeviceEnumeration& enumeration);

  // Called when audio output parameters have been read if needed.
  void PostRequestToUI(
      const std::string& label,
      const MediaDeviceEnumeration& enumeration,
      const std::optional<media::AudioParameters>& output_parameters);

  // Returns true if a device with |device_id| has already been requested with
  // a render_frame_host_id and type equal to the the values
  // in |request|. If it has been requested, |device_info| contain information
  // about the device.
  bool FindExistingRequestedDevice(
      const DeviceRequest& new_request,
      const blink::MediaStreamDevice& new_device,
      blink::MediaStreamDevice* existing_device,
      MediaRequestState* existing_request_state) const;

  void FinalizeGenerateStreams(const std::string& label,
                               DeviceRequest* request);
  void FinalizeGetOpenDevice(const std::string& label, DeviceRequest* request);
  void PanTiltZoomPermissionChecked(
      const std::string& label,
      const std::optional<blink::MediaStreamDevice>& video_device,
      bool pan_tilt_zoom_allowed);
  void FinalizeRequestFailed(DeviceRequests::const_iterator request_it,
                             blink::mojom::MediaStreamRequestResult result);
  void FinalizeChangeDevice(const std::string& label, DeviceRequest* request);
  void FinalizeMediaAccessRequest(
      DeviceRequests::const_iterator request_it,
      const blink::mojom::StreamDevicesSet& stream_devices_set);
  void HandleCheckMediaAccessResponse(const std::string& label,
                                      bool have_access);

  // Picks a device ID from a list of required and alternate device IDs,
  // presented as part of a TrackControls structure.
  // Either the required device ID is picked (if present), or the first
  // valid alternate device ID.
  // Returns false if the required device ID is present and invalid.
  // Otherwise, if no valid device is found, device_id is unchanged.
  bool RemoveInvalidDeviceIds(const MediaDeviceSaltAndOrigin& salt_and_origin,
                              const blink::TrackControls& controls,
                              const blink::WebMediaDeviceInfoArray& devices,
                              std::vector<std::string>* device_id) const;

  // Finds the requested device id from request. The requested device type
  // must be MEDIA_DEVICE_AUDIO_CAPTURE or MEDIA_DEVICE_VIDEO_CAPTURE.
  bool GetEligibleCaptureDeviceids(
      const DeviceRequest* request,
      blink::mojom::MediaStreamType type,
      const blink::WebMediaDeviceInfoArray& devices,
      std::vector<std::string>* device_id) const;

  void TranslateDeviceIdToSourceId(const DeviceRequest* request,
                                   blink::MediaStreamDevice* device) const;

  // Handles the callback from MediaStreamUIProxy to receive the UI window id,
  // used for excluding the notification window in desktop capturing.
  void OnMediaStreamUIWindowId(
      blink::mojom::MediaStreamType video_type,
      blink::mojom::StreamDevicesSetPtr stream_devices_set,
      gfx::NativeViewId window_id);

  // Runs on the IO thread and does the actual [un]registration of callbacks.
  void DoNativeLogCallbackRegistration(
      int renderer_host_id,
      base::RepeatingCallback<void(const std::string&)> callback);
  void DoNativeLogCallbackUnregistration(int renderer_host_id);

  // Callback to handle the reply to a low-level enumeration request.
  void DevicesEnumerated(bool requested_audio_input,
                         bool requested_video_input,
                         const std::string& label,
                         const MediaDeviceEnumeration& enumeration);

  // Creates blink::MediaStreamDevices for |devices_infos| of |stream_type|. For
  // video capture device it also uses cached content from
  // |video_capture_manager_| to set the MediaStreamDevice fields.
  blink::MediaStreamDevices ConvertToMediaStreamDevices(
      blink::mojom::MediaStreamType stream_type,
      const blink::WebMediaDeviceInfoArray& device_infos);

  // Activate the specified tab and bring it to the front.
  void ActivateTabOnUIThread(const DesktopMediaID source);

  // Get the permission controller for a particular RFH. Must be called on the
  // UI thread.
  static PermissionControllerImpl* GetPermissionController(
      GlobalRenderFrameHostId render_frame_host_id);

  void SubscribeToPermissionController(const std::string& label,
                                       const DeviceRequest* request);

  // Subscribe to the permission controller in order to monitor camera/mic
  // permission updates for a particular DeviceRequest. All the additional
  // information is needed because `FindRequest` can't be called on the UI
  // thread.
  void SubscribeToPermissionControllerOnUIThread(
      const std::string& label,
      GlobalRenderFrameHostId requesting_render_frame_host_id,
      int requester_id,
      int page_request_id,
      bool is_audio_request,
      bool is_video_request,
      const GURL& origin);

  // Store the subscription ids on a DeviceRequest in order to allow
  // unsubscribing when the request is deleted.
  void SetPermissionSubscriptionIDs(
      const std::string& label,
      GlobalRenderFrameHostId requesting_render_frame_host_id,
      PermissionController::SubscriptionId audio_subscription_id,
      PermissionController::SubscriptionId video_subscription_id);

  // Unsubscribe from following permission updates for the two specified
  // subscription IDs. Called when a request is deleted.
  static void UnsubscribeFromPermissionControllerOnUIThread(
      GlobalRenderFrameHostId requesting_render_frame_host_id,
      PermissionController::SubscriptionId audio_subscription_id,
      PermissionController::SubscriptionId video_subscription_id);

  // Callback that the PermissionController calls when a permission is updated.
  void PermissionChangedCallback(
      GlobalRenderFrameHostId requesting_render_frame_host_id,
      int requester_id,
      int page_request_id,
      blink::mojom::PermissionStatus status);

  // Start tracking capture-handle changes for tab-capture.
  void MaybeStartTrackingCaptureHandleConfig(
      const std::string& label,
      const blink::MediaStreamDevice& captured_device,
      DeviceRequest& request);

  // Stop tracking capture-handle changes for tab-capture.
  void MaybeStopTrackingCaptureHandleConfig(
      const std::string& label,
      const blink::MediaStreamDevice& captured_device);

  // When device changes, update which tabs' capture-handles are tracked.
  void MaybeUpdateTrackedCaptureHandleConfigs(
      const std::string& label,
      const blink::mojom::StreamDevicesSet& new_stream_devices_set,
      DeviceRequest& request);

  bool ShouldUseFakeUIProxy(const DeviceRequest& request) const;

  std::unique_ptr<MediaStreamUIProxy> MakeFakeUIProxy(
      const std::string& label,
      const MediaDeviceEnumeration& enumeration,
      DeviceRequest* request);

  void GetRawDeviceIdsOpenedForFrameIds(
      blink::mojom::MediaStreamType type,
      GetRawDeviceIdsOpenedForFrameCallback callback,
      base::flat_set<GlobalRenderFrameHostId> render_frame_host_ids) const;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Defines a window of opportunity for the Web-application to decide
  // whether a display-surface which it's capturing should be focused.
  // After |kConditionalFocusWindow| past the beginning of the capture,
  // the browser makes its own decision and ignores further instructions
  // from Web-applications, thereby preventing applications from changing
  // focus at an arbitrary time.
  const base::TimeDelta conditional_focus_window_;

  CapturedSurfaceControllerFactoryCallback captured_surface_controller_factory_;
#endif

  const raw_ptr<media::AudioSystem, DanglingUntriaged>
      audio_system_;  // not owned
  scoped_refptr<AudioInputDeviceManager> audio_input_device_manager_;
  scoped_refptr<VideoCaptureManager> video_capture_manager_;

  std::optional<base::Thread> video_capture_thread_;

  std::unique_ptr<MediaDevicesManager> media_devices_manager_;

  // All non-closed request. Must be accessed on IO thread.
  DeviceRequests requests_;

  // A fake UI factory allows bypassing the user interaction with permission and
  // capture selection dialogs, immediately starting capture with a default
  // selection.
  // Set in unit tests via UseFakeUIFactoryForTests(), and enabled in browser
  // tests / web tests via the command line flag --use-fake-ui-for-media-stream.
  base::RepeatingCallback<std::unique_ptr<FakeMediaStreamUIProxy>(void)>
      fake_ui_factory_;

  // The fake UI doesn't work for getUserMedia desktop captures, so in general
  // we won't use it for them, even if fake_ui_factory_ is set (see
  // crbug.com/919485).
  // Some unittests do still require the fake ui to be used for all captures, so
  // set this indicator to true.
  bool use_fake_ui_for_gum_desktop_capture_ = false;

  // If `true`, the fake UI factory is used only for cameras and microphones,
  // and NOT for any form of screen-capture, regardless if that screen-capture
  // is getDisplayMedia-driven or getUserMedia-driven.
  bool use_fake_ui_only_for_camera_and_microphone_ = false;

  // If `fake_ui_factory_` is set, then when its use results in tab-capture:
  // * If `fake_ui_factory_captured_tab_id_` is also set, it determines the ID
  //   of the tab that will be captured.
  // * Otherwise, the capturing tab will capture itself.
  std::optional<WebContentsMediaCaptureId> fake_ui_factory_captured_tab_id_;

  // Observes changes of captured tabs' CaptureHandleConfig and reports
  // this changes back to their capturers. This object lives on the UI thread
  // and must be accessed on it and torn down from it.
  CaptureHandleManager capture_handle_manager_;

  // Maps render process hosts to log callbacks. Used on the IO thread.
  std::map<int, base::RepeatingCallback<void(const std::string&)>>
      log_callbacks_;

  std::unique_ptr<AudioServiceListener> audio_service_listener_;

  // Provider of system power change logging to the WebRTC logs.
  MediaStreamPowerLogger power_logger_;

  mojo::UniqueReceiverSet<blink::mojom::MediaStreamDispatcherHost>
      dispatcher_hosts_;
  mojo::UniqueReceiverSet<media::mojom::VideoCaptureHost> video_capture_hosts_;

  GenerateStreamTestCallback generate_stream_test_callback_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<media::JpegAcceleratorProviderImpl>
      jpeg_accelerator_provider_;

  std::unique_ptr<media::SystemEventMonitorImpl> system_event_monitor_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_MANAGER_H_
