// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_device_launch_observer.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/screenlock_observer.h"
#include "media/base/video_facing.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {
class VideoCaptureController;
class VideoCaptureControllerEventHandler;

// VideoCaptureManager is used to open/close, start/stop, enumerate available
// video capture devices, and manage VideoCaptureController's.
// In its main usage in production, an instance is created by MediaStreamManager
// on the Browser::IO thread. All public methods are expected to be called from
// the Browser::IO thread. A device can only be opened once.
class CONTENT_EXPORT VideoCaptureManager
    : public MediaStreamProvider,
      public VideoCaptureDeviceLaunchObserver,
      public ScreenlockObserver {
 public:
  using VideoCaptureDevice = media::VideoCaptureDevice;

  // Callback used to signal the completion of a controller lookup.
  using DoneCB =
      base::OnceCallback<void(const base::WeakPtr<VideoCaptureController>&)>;

  using SetDesktopCaptureWindowIdCallback = base::RepeatingCallback<void(
      const media::VideoCaptureSessionId& session_id,
      gfx::NativeViewId window_id)>;

  explicit VideoCaptureManager(
      std::unique_ptr<VideoCaptureProvider> video_capture_provider,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);

  VideoCaptureManager(const VideoCaptureManager&) = delete;
  VideoCaptureManager& operator=(const VideoCaptureManager&) = delete;

  // AddVideoCaptureObserver() can be called only before any devices are opened.
  // RemoveAllVideoCaptureObservers() can be called only after all devices
  // are closed.
  // They can be called more than once and it's ok to not call at all if the
  // client is not interested in receiving media::VideoCaptureObserver callacks.
  // This methods can be called on whatever thread. The callbacks of
  // media::VideoCaptureObserver arrive on browser IO thread.
  void AddVideoCaptureObserver(media::VideoCaptureObserver* observer);
  void RemoveAllVideoCaptureObservers();

  // Implements MediaStreamProvider.
  void RegisterListener(MediaStreamProviderListener* listener) override;
  void UnregisterListener(MediaStreamProviderListener* listener) override;
  base::UnguessableToken Open(const blink::MediaStreamDevice& device) override;
  void Close(const base::UnguessableToken& capture_session_id) override;

  // Start/stop cropping/restricting the video track.
  //
  // Non-empty |target| sets (or changes) the sub-capture target.
  // Empty |target| reverts the capture to its original state.
  //
  // |sub_capture_target_version| must be incremented by at least one for each
  // call. By including it in frame's metadata, Viz informs Blink what was the
  // latest invocation of cropTo() or restrictTo() before a given frame was
  // produced.
  //
  // The callback reports success/failure.
  void ApplySubCaptureTarget(
      const base::UnguessableToken& session_id,
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
          callback);

  // Called by VideoCaptureHost to locate a capture device for `capture_params`,
  // adding the Host as a client of the device's controller if successful. The
  // value of `session_id` controls which device is selected;
  // this value should be a session id previously returned by Open().
  //
  // If the device is not already started (i.e., no other client is currently
  // capturing from this device), this call will cause a VideoCaptureController
  // and VideoCaptureDevice to be created, possibly asynchronously.
  //
  // On success, the controller is returned via calling |done_cb|, indicating
  // that the client was successfully added. A NULL controller is passed to
  // the callback on failure. `done_cb` is not allowed to synchronously call
  // StopCaptureForClient().
  //
  // `browser_context` is used to access the `MediaEffectsService` and pass a
  // `VideoEffectsProcessor` remote for this device to the
  // `VideoCaptureDeviceClient`. If the `browser_context` is nullptr then the
  // device won't get an effects processor.
  void ConnectClient(const media::VideoCaptureSessionId& session_id,
                     const media::VideoCaptureParams& capture_params,
                     VideoCaptureControllerID client_id,
                     VideoCaptureControllerEventHandler* client_handler,
                     std::optional<url::Origin> origin,
                     DoneCB done_cb,
                     BrowserContext* browser_context);

  // Called by VideoCaptureHost to remove |client_handler|. If this is the last
  // client of the device, the |controller| and its VideoCaptureDevice may be
  // destroyed. The client must not access |controller| after calling this
  // function.
  void DisconnectClient(VideoCaptureController* controller,
                        VideoCaptureControllerID client_id,
                        VideoCaptureControllerEventHandler* client_handler,
                        media::VideoCaptureError error);

  // Called by VideoCaptureHost to pause to update video buffer specified by
  // |client_id| and |client_handler|. If all clients of |controller| are
  // paused, the corresponding device will be closed.
  void PauseCaptureForClient(
      VideoCaptureController* controller,
      VideoCaptureControllerID client_id,
      VideoCaptureControllerEventHandler* client_handler);

  // Called by VideoCaptureHost to resume to update video buffer specified by
  // |client_id| and |client_handler|. The |session_id| and |params| should be
  // same as those used in ConnectClient().
  // If this is first active client of |controller|, device will be allocated
  // and it will take a little time to resume.
  // Allocating device could failed if other app holds the camera, the error
  // will be notified through VideoCaptureControllerEventHandler::OnError().
  void ResumeCaptureForClient(
      const media::VideoCaptureSessionId& session_id,
      const media::VideoCaptureParams& params,
      VideoCaptureController* controller,
      VideoCaptureControllerID client_id,
      VideoCaptureControllerEventHandler* client_handler);

  // Called by VideoCaptureHost to request a refresh frame from the video
  // capture device.
  void RequestRefreshFrameForClient(VideoCaptureController* controller);

  // Retrieves all capture supported formats for a particular device. Returns
  // false if the |capture_session_id| is not found. The supported formats are
  // cached during device(s) enumeration, and depending on the underlying
  // implementation, could be an empty list.
  bool GetDeviceSupportedFormats(
      const media::VideoCaptureSessionId& capture_session_id,
      media::VideoCaptureFormats* supported_formats);
  // Retrieves all capture supported formats for a particular device. Returns
  // false if the  |device_id| is not found. The supported formats are cached
  // during device(s) enumeration, and depending on the underlying
  // implementation, could be an empty list.
  bool GetDeviceSupportedFormats(const std::string& device_id,
                                 media::VideoCaptureFormats* supported_formats);

  // Retrieves the format(s) currently in use.  Returns false if the
  // |capture_session_id| is not found. Returns true and |formats_in_use|
  // otherwise. |formats_in_use| is empty if the device is not in use.
  bool GetDeviceFormatsInUse(
      const media::VideoCaptureSessionId& capture_session_id,
      media::VideoCaptureFormats* formats_in_use);
  // Retrieves the format currently in use.  Returns std::nullopt if the
  // |stream_type|, |device_id| pair is not found. Returns in-use format of the
  // device otherwise.
  std::optional<media::VideoCaptureFormat> GetDeviceFormatInUse(
      blink::mojom::MediaStreamType stream_type,
      const std::string& device_id);

  // If there is a capture session associated with |session_id|, and the
  // captured entity a tab, return the GlobalRenderFrameHostId of
  // the captured tab.
  // Otherwise, returns an empty GlobalRenderFrameHostId.
  GlobalRenderFrameHostId GetGlobalRenderFrameHostId(
      const base::UnguessableToken& session_id) const;

  // Sets the platform-dependent window ID for the desktop capture notification
  // UI for the given session.
  void SetDesktopCaptureWindowId(const media::VideoCaptureSessionId& session_id,
                                 gfx::NativeViewId window_id);

  void GetPhotoState(const base::UnguessableToken& session_id,
                     VideoCaptureDevice::GetPhotoStateCallback callback);
  void SetPhotoOptions(const base::UnguessableToken& session_id,
                       media::mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);
  void TakePhoto(const base::UnguessableToken& session_id,
                 VideoCaptureDevice::TakePhotoCallback callback);

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
  // Some devices had troubles when stopped and restarted quickly, so the device
  // is only stopped when Chrome is sent to background and not when, e.g., a tab
  // is hidden, see http://crbug.com/582295.
  void OnApplicationStateChange(base::android::ApplicationState state);
#endif

  using EnumerationCallback =
      base::OnceCallback<void(media::mojom::DeviceEnumerationResult result_code,
                              const media::VideoCaptureDeviceDescriptors&)>;
  // Asynchronously obtains descriptors for the available devices.
  // As a side-effect, updates |devices_info_cache_|.
  void EnumerateDevices(EnumerationCallback client_callback);

  // VideoCaptureDeviceLaunchObserver implementation:
  void OnDeviceLaunched(VideoCaptureController* controller) override;
  void OnDeviceLaunchFailed(VideoCaptureController* controller,
                            media::VideoCaptureError error) override;
  void OnDeviceLaunchAborted() override;
  void OnDeviceConnectionLost(VideoCaptureController* controller) override;

  void OpenNativeScreenCapturePicker(
      DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
      base::OnceCallback<void()> cancel_callback,
      base::OnceCallback<void()> error_callback);

  void CloseNativeScreenCapturePicker(DesktopMediaID device_id);

  bool is_idle_close_timer_running_for_testing() const {
    return idle_close_timer_.IsRunning();
  }
  void set_idle_close_timeout_for_testing(base::TimeDelta timeout) {
    idle_close_timeout_ = timeout;
  }

  void set_desktop_capture_window_id_callback_for_testing(
      SetDesktopCaptureWindowIdCallback callback) {
    set_desktop_capture_window_id_callback_for_testing_ = callback;
  }

 private:
  class CaptureDeviceStartRequest;

  using SessionMap =
      std::map<media::VideoCaptureSessionId, blink::MediaStreamDevice>;
  using DeviceStartQueue = base::circular_deque<CaptureDeviceStartRequest>;
  using VideoCaptureDeviceDescriptor = media::VideoCaptureDeviceDescriptor;
  using VideoCaptureDeviceDescriptors = media::VideoCaptureDeviceDescriptors;

  ~VideoCaptureManager() override;

  void OnDeviceInfosReceived(
      base::ElapsedTimer timer,
      EnumerationCallback client_callback,
      media::mojom::DeviceEnumerationResult error_code,
      const std::vector<media::VideoCaptureDeviceInfo>& device_infos);

  // Helpers to report an event to our Listener.
  void OnOpened(blink::mojom::MediaStreamType type,
                const media::VideoCaptureSessionId& capture_session_id);
  void OnClosed(blink::mojom::MediaStreamType type,
                const media::VideoCaptureSessionId& capture_session_id);

  // Checks to see if |controller| has no clients left. If so, remove it from
  // the list of controllers, and delete it asynchronously. |controller| may be
  // freed by this function.
  void DestroyControllerIfNoClients(
      const base::UnguessableToken& capture_session_id,
      VideoCaptureController* controller);

  // Finds a VideoCaptureController in different ways: by |session_id|, by its
  // |device_id| and |type| (if it is already opened), by its |controller| or by
  // its |serial_id|. In all cases, if not found, nullptr is returned.
  VideoCaptureController* LookupControllerBySessionId(
      const base::UnguessableToken& session_id) const;
  VideoCaptureController* LookupControllerByMediaTypeAndDeviceId(
      blink::mojom::MediaStreamType type,
      const std::string& device_id) const;
  bool IsControllerPointerValid(const VideoCaptureController* controller) const;
  scoped_refptr<VideoCaptureController> GetControllerSharedRef(
      VideoCaptureController* controller) const;

  // Finds the device info by |id| in |devices_info_cache_|, or nullptr.
  media::VideoCaptureDeviceInfo* GetDeviceInfoById(const std::string& id);

  // Finds a VideoCaptureController for the indicated |capture_session_id|,
  // creating a fresh one if necessary. Returns nullptr if said
  // |capture_session_id| is invalid.
  VideoCaptureController* GetOrCreateController(
      const media::VideoCaptureSessionId& capture_session_id,
      const media::VideoCaptureParams& params);

  // Starting a capture device can take 1-2 seconds.
  // To avoid multiple unnecessary start/stop commands to the OS, each start
  // request is queued in |device_start_request_queue_|.
  // QueueStartDevice creates a new entry in |device_start_request_queue_| and
  // posts a request to start the device on the device thread unless there is
  // another request pending start.
  void QueueStartDevice(
      const media::VideoCaptureSessionId& session_id,
      VideoCaptureController* controller,
      const media::VideoCaptureParams& params,
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor);
  void DoStopDevice(VideoCaptureController* controller);
  void ProcessDeviceStartRequestQueue();

  void MaybePostDesktopCaptureWindowId(
      const media::VideoCaptureSessionId& session_id);

  void ReleaseDevices();
  void ResumeDevices();

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  bool application_state_has_running_activities_;
#endif

  // ScreenlockObserver implementation:
  void OnScreenLocked() override;
  void OnScreenUnlocked() override;

  void EmitLogMessage(const std::string& message, int verbose_log_level);

  // Only accessed on Browser::IO thread.
  base::ObserverList<MediaStreamProviderListener>::UncheckedAndDanglingUntriaged
      listeners_;

  // An entry is kept in this map for every session that has been created via
  // the Open() entry point. The keys are session_id's. This map is used to
  // determine which device to use when ConnectClient() occurs. Used
  // only on the IO thread.
  SessionMap sessions_;

  // A set of sessions that have encountered screen lock.
  base::flat_set<media::VideoCaptureSessionId> locked_sessions_;
  base::TimeTicks lock_time_;

  // Currently opened VideoCaptureController instances. The device may or may
  // not be started. This member is only accessed on IO thread.
  std::vector<scoped_refptr<VideoCaptureController>> controllers_;

  // TODO(chfremer): Consider using CancellableTaskTracker, see
  // crbug.com/598465.
  DeviceStartQueue device_start_request_queue_;

  // Queue to keep photo-associated requests waiting for a device to initialize,
  // bundles a session id token and an associated photo-related request.
  base::circular_deque<std::pair<base::UnguessableToken, base::OnceClosure>>
      photo_request_queue_;

  const std::unique_ptr<VideoCaptureProvider> video_capture_provider_;
  base::RepeatingCallback<void(const std::string&)> emit_log_message_cb_;

  base::ObserverList<media::VideoCaptureObserver>::UncheckedAndDanglingUntriaged
      capture_observers_;

  // Local cache of the enumerated DeviceInfos. GetDeviceSupportedFormats() will
  // use this list if the device is not started, otherwise it will retrieve the
  // active device capture format from the VideoCaptureController associated.
  std::vector<media::VideoCaptureDeviceInfo> devices_info_cache_;

  // Map used by DesktopCapture.
  std::map<media::VideoCaptureSessionId, gfx::NativeViewId>
      notification_window_ids_;

  // Closes video device capture sessions after a timeout. Idle timeout value
  // chosen based on UMA metrics. See https://crbug.com/1163105#c28
  base::TimeDelta idle_close_timeout_ = base::Seconds(15);
  base::OneShotTimer idle_close_timer_;

  SetDesktopCaptureWindowIdCallback
      set_desktop_capture_window_id_callback_for_testing_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
