// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_PROCESSOR_H_

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/public/web/web_user_media_request.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_audio.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace gfx {
class Size;
}

namespace blink {
class AudioCaptureSettings;
class LocalFrame;
class MediaStreamAudioSource;
class MediaStreamVideoSource;
class VideoCaptureSettings;
class WebMediaStream;
class WebMediaStreamDeviceObserver;
class WebMediaStreamSource;
class WebString;

// TODO(guidou): Add |request_id| and |is_processing_user_gesture| to
// blink::WebUserMediaRequest and remove this struct.
struct UserMediaRequestInfo {
  UserMediaRequestInfo(int request_id,
                       const blink::WebUserMediaRequest& web_request,
                       bool is_processing_user_gesture);
  const int request_id;
  const blink::WebUserMediaRequest web_request;
  const bool is_processing_user_gesture;
};

// UserMediaProcessor is responsible for processing getUserMedia() requests.
// It also keeps tracks of all sources used by streams created with
// getUserMedia().
// It communicates with the browser via MediaStreamDispatcherHost.
// Only one MediaStream at a time can be in the process of being created.
// UserMediaProcessor must be created, called and destroyed on the main
// render thread. There should be only one UserMediaProcessor per frame.
class MODULES_EXPORT UserMediaProcessor
    : public GarbageCollected<UserMediaProcessor> {
 public:
  using MediaDevicesDispatcherCallback = base::RepeatingCallback<
      blink::mojom::blink::MediaDevicesDispatcherHost*()>;
  // |web_frame| must outlive this instance.
  UserMediaProcessor(LocalFrame* frame,
                     MediaDevicesDispatcherCallback media_devices_dispatcher_cb,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~UserMediaProcessor();

  // It can be assumed that the output of CurrentRequest() remains the same
  // during the execution of a task on the main thread unless ProcessRequest or
  // DeleteWebRequest are invoked.
  // TODO(guidou): Remove this method. https://crbug.com/764293
  UserMediaRequestInfo* CurrentRequest();

  // Starts processing |request| in order to create a new MediaStream. When
  // processing of |request| is complete, it notifies by invoking |callback|.
  // This method must be called only if there is no request currently being
  // processed.
  void ProcessRequest(std::unique_ptr<UserMediaRequestInfo> request,
                      base::OnceClosure callback);

  // If |web_request| is the request currently being processed, stops processing
  // the request and returns true. Otherwise, performs no action and returns
  // false.
  // TODO(guidou): Make this method private and replace with a public
  // CancelRequest() method that deletes the request only if it has not been
  // generated yet. https://crbug.com/764293
  bool DeleteWebRequest(const blink::WebUserMediaRequest& web_request);

  // Stops processing the current request, if any, and stops all sources
  // currently being tracked, effectively stopping all tracks associated with
  // those sources.
  void StopAllProcessing();

  bool HasActiveSources() const;

  void OnDeviceStopped(const blink::MediaStreamDevice& device);
  void OnDeviceChanged(const blink::MediaStreamDevice& old_device,
                       const blink::MediaStreamDevice& new_device);

  void set_media_stream_dispatcher_host_for_testing(
      mojo::PendingRemote<blink::mojom::blink::MediaStreamDispatcherHost>
          dispatcher_host) {
    dispatcher_host_.Bind(std::move(dispatcher_host));
  }

  void Trace(Visitor*);

 protected:
  // These methods are virtual for test purposes. A test can override them to
  // test requesting local media streams. The function notifies WebKit that the
  // |request| have completed.
  virtual void GetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest web_request);
  virtual void GetUserMediaRequestFailed(
      blink::mojom::blink::MediaStreamRequestResult result,
      const String& constraint_name = String());

  // Creates a MediaStreamAudioSource/MediaStreamVideoSource objects.
  // These are virtual for test purposes.
  virtual std::unique_ptr<blink::MediaStreamAudioSource> CreateAudioSource(
      const blink::MediaStreamDevice& device,
      blink::WebPlatformMediaStreamSource::ConstraintsRepeatingCallback
          source_ready);
  virtual std::unique_ptr<blink::MediaStreamVideoSource> CreateVideoSource(
      const blink::MediaStreamDevice& device,
      blink::WebPlatformMediaStreamSource::SourceStoppedCallback stop_callback);

  // Intended to be used only for testing.
  const blink::AudioCaptureSettings& AudioCaptureSettingsForTesting() const;
  const blink::VideoCaptureSettings& VideoCaptureSettingsForTesting() const;

  void SetMediaStreamDeviceObserverForTesting(
      WebMediaStreamDeviceObserver* media_stream_device_observer);

 private:
  class RequestInfo;
  using LocalStreamSources = Vector<blink::WebMediaStreamSource>;

  void OnStreamGenerated(int request_id,
                         blink::mojom::blink::MediaStreamRequestResult result,
                         const String& label,
                         const Vector<blink::MediaStreamDevice>& audio_devices,
                         const Vector<blink::MediaStreamDevice>& video_devices);

  void GotAllVideoInputFormatsForDevice(
      const blink::WebUserMediaRequest& web_request,
      const String& label,
      const String& device_id,
      const Vector<media::VideoCaptureFormat>& formats);

  gfx::Size GetScreenSize();

  void OnStreamGenerationFailed(
      int request_id,
      blink::mojom::blink::MediaStreamRequestResult result);

  bool IsCurrentRequestInfo(int request_id) const;
  bool IsCurrentRequestInfo(
      const blink::WebUserMediaRequest& web_request) const;
  void DelayedGetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest web_request);
  void DelayedGetUserMediaRequestFailed(
      blink::WebUserMediaRequest web_request,
      blink::mojom::blink::MediaStreamRequestResult result,
      const String& constraint_name);

  // Called when |source| has been stopped from JavaScript.
  void OnLocalSourceStopped(const blink::WebMediaStreamSource& source);

  // Creates a WebKit representation of a stream source based on
  // |device| from the MediaStreamDispatcherHost.
  blink::WebMediaStreamSource InitializeVideoSourceObject(
      const blink::MediaStreamDevice& device);

  blink::WebMediaStreamSource InitializeAudioSourceObject(
      const blink::MediaStreamDevice& device,
      bool* is_pending);

  void StartTracks(const String& label);

  void CreateVideoTracks(const Vector<blink::MediaStreamDevice>& devices,
                         Vector<blink::WebMediaStreamTrack>* webkit_tracks);

  void CreateAudioTracks(const Vector<blink::MediaStreamDevice>& devices,
                         Vector<blink::WebMediaStreamTrack>* webkit_tracks);

  // Callback function triggered when all native versions of the
  // underlying media sources and tracks have been created and started.
  void OnCreateNativeTracksCompleted(
      const String& label,
      RequestInfo* request,
      blink::mojom::blink::MediaStreamRequestResult result,
      const String& result_name);

  void OnStreamGeneratedForCancelledRequest(
      const Vector<blink::MediaStreamDevice>& audio_devices,
      const Vector<blink::MediaStreamDevice>& video_devices);

  static void OnAudioSourceStartedOnAudioThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      UserMediaProcessor* weak_ptr,
      blink::WebPlatformMediaStreamSource* source,
      blink::mojom::blink::MediaStreamRequestResult result,
      const blink::WebString& result_name);

  void OnAudioSourceStarted(
      blink::WebPlatformMediaStreamSource* source,
      blink::mojom::blink::MediaStreamRequestResult result,
      const String& result_name);

  void NotifyCurrentRequestInfoOfAudioSourceStarted(
      blink::WebPlatformMediaStreamSource* source,
      blink::mojom::blink::MediaStreamRequestResult result,
      const String& result_name);

  void DeleteAllUserMediaRequests();

  // Returns the source that use a device with |device.session_id|
  // and |device.device.id|. nullptr if such source doesn't exist.
  const blink::WebMediaStreamSource* FindLocalSource(
      const blink::MediaStreamDevice& device) const {
    return FindLocalSource(local_sources_, device);
  }
  const blink::WebMediaStreamSource* FindPendingLocalSource(
      const blink::MediaStreamDevice& device) const {
    return FindLocalSource(pending_local_sources_, device);
  }
  const blink::WebMediaStreamSource* FindLocalSource(
      const LocalStreamSources& sources,
      const blink::MediaStreamDevice& device) const;

  // Looks up a local source and returns it if found. If not found, prepares
  // a new WebMediaStreamSource with a nullptr extraData pointer.
  blink::WebMediaStreamSource FindOrInitializeSourceObject(
      const blink::MediaStreamDevice& device);

  // Returns true if we do find and remove the |source|.
  // Otherwise returns false.
  bool RemoveLocalSource(const blink::WebMediaStreamSource& source);

  void StopLocalSource(const blink::WebMediaStreamSource& source,
                       bool notify_dispatcher);

  blink::mojom::blink::MediaStreamDispatcherHost*
  GetMediaStreamDispatcherHost();
  blink::mojom::blink::MediaDevicesDispatcherHost* GetMediaDevicesDispatcher();

  void SetupAudioInput();
  void SelectAudioDeviceSettings(
      const blink::WebUserMediaRequest& web_request,
      Vector<blink::mojom::blink::AudioInputDeviceCapabilitiesPtr>
          audio_input_capabilities);
  void SelectAudioSettings(
      const blink::WebUserMediaRequest& web_request,
      const blink::AudioDeviceCaptureCapabilities& capabilities);

  void SetupVideoInput();
  void SelectVideoDeviceSettings(
      const blink::WebUserMediaRequest& web_request,
      Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr>
          video_input_capabilities);
  void FinalizeSelectVideoDeviceSettings(
      const blink::WebUserMediaRequest& web_request,
      const blink::VideoCaptureSettings& settings);
  void SelectVideoContentSettings();

  base::Optional<base::UnguessableToken> DetermineExistingAudioSessionId();

  void GenerateStreamForCurrentRequestInfo(
      base::Optional<base::UnguessableToken>
          requested_audio_capture_session_id = base::nullopt,
      blink::mojom::StreamSelectionStrategy strategy =
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID);

  WebMediaStreamDeviceObserver* GetMediaStreamDeviceObserver();

  // Owned by the test.
  WebMediaStreamDeviceObserver* media_stream_device_observer_for_testing_ =
      nullptr;

  LocalStreamSources local_sources_;
  LocalStreamSources pending_local_sources_;

  mojo::Remote<blink::mojom::blink::MediaStreamDispatcherHost> dispatcher_host_;

  // UserMedia requests are processed sequentially. |current_request_info_|
  // contains the request currently being processed.
  Member<RequestInfo> current_request_info_;
  MediaDevicesDispatcherCallback media_devices_dispatcher_cb_;
  base::OnceClosure request_completed_cb_;

  Member<LocalFrame> frame_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(UserMediaProcessor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_PROCESSOR_H_
