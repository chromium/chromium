// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/apply_constraints_processor.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

static int32_t g_next_request_id = 0;

// The histogram counts the number of calls to the JS APIs
// getUserMedia() and getDisplayMedia().
void UpdateAPICount(UserMediaRequestType media_type) {
  RTCAPIName api_name = RTCAPIName::kGetUserMedia;
  switch (media_type) {
    case UserMediaRequestType::kUserMedia:
      api_name = RTCAPIName::kGetUserMedia;
      break;
    case UserMediaRequestType::kDisplayMedia:
      api_name = RTCAPIName::kGetDisplayMedia;
      break;
    case UserMediaRequestType::kAllScreensMedia:
      api_name = RTCAPIName::kGetAllScreensMedia;
      break;
  }
  UpdateWebRTCMethodCount(api_name);
}

}  // namespace

// RequestQueue holds a queue of pending requests that can be processed
// independently from other types of requests. It keeps individual processor
// objects so that the processing state is kept separated between requests that
// are processed in parallel.
class UserMediaClient::RequestQueue final
    : public GarbageCollected<UserMediaClient::RequestQueue> {
 public:
  RequestQueue(LocalFrame* frame,
               UserMediaProcessor* user_media_processor,
               UserMediaClient* user_media_client,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~RequestQueue();

  void EnqueueAndMaybeProcess(Request* request);
  bool IsCapturing() { return user_media_processor_->HasActiveSources(); }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void FocusCapturedSurface(const String& label, bool focus) {
    user_media_processor_->FocusCapturedSurface(label, focus);
  }
#endif

  void CancelUserMediaRequest(UserMediaRequest* user_media_request);
  void DeleteAllUserMediaRequests();
  void KeepDeviceAliveForTransfer(
      base::UnguessableToken session_id,
      base::UnguessableToken transfer_id,
      UserMediaProcessor::KeepDeviceAliveForTransferCallback keep_alive_cb);

  void Trace(Visitor* visitor) const;

 private:
  void MaybeProcessNextRequestInfo();
  void CurrentRequestCompleted();

  WeakMember<LocalFrame> frame_;
  Member<UserMediaProcessor> user_media_processor_;
  Member<ApplyConstraintsProcessor> apply_constraints_processor_;

  // UserMedia requests enqueued on the same RequestQueue are processed
  // sequentially. |is_processing_request_| is a flag that indicates if a
  // request is being processed at a given time, and |pending_request_infos_| is
  // a list of the queued requests.
  bool is_processing_request_ = false;

  HeapDeque<Member<Request>> pending_requests_;
  THREAD_CHECKER(thread_checker_);
};

UserMediaClient::RequestQueue::RequestQueue(
    LocalFrame* frame,
    UserMediaProcessor* user_media_processor,
    UserMediaClient* user_media_client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : frame_(frame),
      user_media_processor_(user_media_processor),
      apply_constraints_processor_(
          MakeGarbageCollected<ApplyConstraintsProcessor>(
              frame,
              WTF::BindRepeating(
                  [](UserMediaClient* client)
                      -> mojom::blink::MediaDevicesDispatcherHost* {
                    // |client| is guaranteed to be not null because |client|
                    // transitively owns this ApplyConstraintsProcessor.
                    DCHECK(client);
                    return client->GetMediaDevicesDispatcher();
                  },
                  WrapWeakPersistent(user_media_client)),
              std::move(task_runner))) {
  DCHECK(frame_);
  DCHECK(user_media_processor_);
}

UserMediaClient::RequestQueue::~RequestQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Ensure that ContextDestroyed() gets called before the destructor.
  DCHECK(!is_processing_request_);
}

void UserMediaClient::RequestQueue::EnqueueAndMaybeProcess(Request* request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pending_requests_.push_back(request);
  if (!is_processing_request_)
    MaybeProcessNextRequestInfo();
}

void UserMediaClient::RequestQueue::CancelUserMediaRequest(
    UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    // TODO(guidou): Remove this conditional logging. https://crbug.com/764293
    UserMediaRequest* request = user_media_processor_->CurrentRequest();
    if (request == user_media_request) {
      blink::WebRtcLogMessage(
          base::StringPrintf("UMCI::CancelUserMediaRequest. request_id=%d",
                             request->request_id()));
    }
  }

  if (!user_media_processor_->DeleteUserMediaRequest(user_media_request)) {
    for (auto it = pending_requests_.begin(); it != pending_requests_.end();
         ++it) {
      if ((*it)->IsUserMedia() &&
          (*it)->user_media_request() == user_media_request) {
        pending_requests_.erase(it);
        break;
      }
    }
  }
}

void UserMediaClient::RequestQueue::DeleteAllUserMediaRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  user_media_processor_->StopAllProcessing();
  is_processing_request_ = false;
  pending_requests_.clear();
}

void UserMediaClient::RequestQueue::KeepDeviceAliveForTransfer(
    base::UnguessableToken session_id,
    base::UnguessableToken transfer_id,
    UserMediaProcessor::KeepDeviceAliveForTransferCallback keep_alive_cb) {
  // KeepDeviceAliveForTransfer is safe to call even during an ongoing request,
  // so doesn't need to be queued
  user_media_processor_->KeepDeviceAliveForTransfer(session_id, transfer_id,
                                                    std::move(keep_alive_cb));
}

void UserMediaClient::RequestQueue::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(user_media_processor_);
  visitor->Trace(apply_constraints_processor_);
  visitor->Trace(pending_requests_);
}

void UserMediaClient::RequestQueue::MaybeProcessNextRequestInfo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_processing_request_ || pending_requests_.empty())
    return;

  auto current_request = std::move(pending_requests_.front());
  pending_requests_.pop_front();
  is_processing_request_ = true;

  if (current_request->IsUserMedia()) {
    user_media_processor_->ProcessRequest(
        current_request->MoveUserMediaRequest(),
        WTF::BindOnce(&UserMediaClient::RequestQueue::CurrentRequestCompleted,
                      WrapWeakPersistent(this)));
  } else if (current_request->IsApplyConstraints()) {
    apply_constraints_processor_->ProcessRequest(
        current_request->apply_constraints_request(),
        WTF::BindOnce(&UserMediaClient::RequestQueue::CurrentRequestCompleted,
                      WrapWeakPersistent(this)));
  } else {
    DCHECK(current_request->IsStopTrack());
    MediaStreamTrackPlatform* track = MediaStreamTrackPlatform::GetTrack(
        WebMediaStreamTrack(current_request->track_to_stop()));
    if (track) {
      track->StopAndNotify(
          WTF::BindOnce(&UserMediaClient::RequestQueue::CurrentRequestCompleted,
                        WrapWeakPersistent(this)));
    } else {
      CurrentRequestCompleted();
    }
  }
}

void UserMediaClient::RequestQueue::CurrentRequestCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_processing_request_ = false;
  if (!pending_requests_.empty()) {
    frame_->GetTaskRunner(blink::TaskType::kInternalMedia)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(
                &UserMediaClient::RequestQueue::MaybeProcessNextRequestInfo,
                WrapWeakPersistent(this)));
  }
}

UserMediaClient::Request::Request(UserMediaRequest* user_media_request)
    : user_media_request_(user_media_request) {
  DCHECK(user_media_request_);
  DCHECK(!apply_constraints_request_);
  DCHECK(!track_to_stop_);
}

UserMediaClient::Request::Request(blink::ApplyConstraintsRequest* request)
    : apply_constraints_request_(request) {
  DCHECK(apply_constraints_request_);
  DCHECK(!user_media_request_);
  DCHECK(!track_to_stop_);
}

UserMediaClient::Request::Request(MediaStreamComponent* track_to_stop)
    : track_to_stop_(track_to_stop) {
  DCHECK(track_to_stop_);
  DCHECK(!user_media_request_);
  DCHECK(!apply_constraints_request_);
}

UserMediaClient::Request::~Request() = default;

UserMediaRequest* UserMediaClient::Request::MoveUserMediaRequest() {
  auto user_media_request = user_media_request_;
  user_media_request_ = nullptr;
  return user_media_request.Get();
}

UserMediaClient::UserMediaClient(
    LocalFrame* frame,
    UserMediaProcessor* user_media_processor,
    UserMediaProcessor* display_user_media_processor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : Supplement<LocalDOMWindow>(*frame->DomWindow()),
      ExecutionContextLifecycleObserver(frame->DomWindow()),
      frame_(frame),
      media_devices_dispatcher_(frame->DomWindow()),
      pending_device_requests_(
          MakeGarbageCollected<RequestQueue>(frame,
                                             user_media_processor,
                                             this,
                                             task_runner)),
      pending_display_requests_(
          MakeGarbageCollected<RequestQueue>(frame,
                                             display_user_media_processor,
                                             this,
                                             task_runner)) {
  CHECK(frame_);

  // WrapWeakPersistent is safe because the |frame_| owns UserMediaClient.
  frame_->SetIsCapturingMediaCallback(WTF::BindRepeating(
      [](UserMediaClient* client) { return client && client->IsCapturing(); },
      WrapWeakPersistent(this)));
}

UserMediaClient::UserMediaClient(
    LocalFrame* frame,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : UserMediaClient(
          frame,
          MakeGarbageCollected<UserMediaProcessor>(
              frame,
              WTF::BindRepeating(
                  [](UserMediaClient* client)
                      -> mojom::blink::MediaDevicesDispatcherHost* {
                    // |client| is guaranteed to be not null because |client|
                    // owns transitively this UserMediaProcessor.
                    DCHECK(client);
                    return client->GetMediaDevicesDispatcher();
                  },
                  WrapWeakPersistent(this)),
              frame->GetTaskRunner(blink::TaskType::kInternalMedia)),
          MakeGarbageCollected<UserMediaProcessor>(
              frame,
              WTF::BindRepeating(
                  [](UserMediaClient* client)
                      -> mojom::blink::MediaDevicesDispatcherHost* {
                    // |client| is guaranteed to be not null because
                    // |client| transitively owns this UserMediaProcessor.
                    DCHECK(client);
                    return client->GetMediaDevicesDispatcher();
                  },
                  WrapWeakPersistent(this)),
              frame->GetTaskRunner(blink::TaskType::kInternalMedia)),
          std::move(task_runner)) {}

void UserMediaClient::RequestUserMedia(UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(user_media_request);
  DCHECK(user_media_request->Audio() || user_media_request->Video());
  // GetWindow() may be null if we are in a test.
  // In that case, it's OK to not check frame().
  DCHECK(!user_media_request->GetWindow() ||
         frame_ == user_media_request->GetWindow()->GetFrame());

  // Save histogram data so we can see how much GetUserMedia is used.
  UpdateAPICount(user_media_request->MediaRequestType());

  int32_t request_id = g_next_request_id++;
  blink::WebRtcLogMessage(base::StringPrintf(
      "UMCI::RequestUserMedia({request_id=%d}, {audio constraints=%s}, "
      "{video constraints=%s})",
      request_id,
      user_media_request->AudioConstraints().ToString().Utf8().c_str(),
      user_media_request->VideoConstraints().ToString().Utf8().c_str()));

  // The value returned by HasTransientUserActivation() is used by the browser
  // to make decisions about the permissions UI. Its value can be lost while
  // switching threads, so saving its value here.
  //
  // TODO(mustaq): The description above seems specific to pre-UAv2 stack-based
  // tokens.  Perhaps we don't need to preserve this bit?
  bool has_transient_user_activation = false;
  if (LocalDOMWindow* window = user_media_request->GetWindow()) {
    has_transient_user_activation =
        LocalFrame::HasTransientUserActivation(window->GetFrame());
  }
  user_media_request->set_request_id(request_id);

  // TODO(crbug.com/787254): Communicate directly with the
  // PeerConnectionTrackerHost mojo object once it is available from Blink.
  if (auto* window = user_media_request->GetWindow()) {
    if (user_media_request->MediaRequestType() ==
        UserMediaRequestType::kUserMedia) {
      PeerConnectionTracker::From(*window).TrackGetUserMedia(
          user_media_request);
    } else {
      PeerConnectionTracker::From(*window).TrackGetDisplayMedia(
          user_media_request);
    }
  }

  user_media_request->set_has_transient_user_activation(
      has_transient_user_activation);
  mojom::blink::MediaStreamType type =
      user_media_request->Video() ? user_media_request->VideoMediaStreamType()
                                  : user_media_request->AudioMediaStreamType();
  auto* queue = GetRequestQueue(type);
  queue->EnqueueAndMaybeProcess(
      MakeGarbageCollected<Request>(user_media_request));
}

void UserMediaClient::ApplyConstraints(
    blink::ApplyConstraintsRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(user_media_request);
  DCHECK(user_media_request->Track());
  DCHECK(user_media_request->Track()->Source());
  DCHECK(user_media_request->Track()->Source()->GetPlatformSource());
  auto* queue = GetRequestQueue(user_media_request->Track()
                                    ->Source()
                                    ->GetPlatformSource()
                                    ->device()
                                    .type);
  queue->EnqueueAndMaybeProcess(
      MakeGarbageCollected<Request>(user_media_request));
}

void UserMediaClient::StopTrack(MediaStreamComponent* track) {
  DCHECK(track);
  DCHECK(track->Source());
  DCHECK(track->Source()->GetPlatformSource());
  auto* queue =
      GetRequestQueue(track->Source()->GetPlatformSource()->device().type);
  queue->EnqueueAndMaybeProcess(MakeGarbageCollected<Request>(track));
}

bool UserMediaClient::IsCapturing() {
  return pending_device_requests_->IsCapturing() ||
         pending_display_requests_->IsCapturing();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void UserMediaClient::FocusCapturedSurface(const String& label, bool focus) {
  pending_display_requests_->FocusCapturedSurface(label, focus);
}
#endif

void UserMediaClient::CancelUserMediaRequest(
    UserMediaRequest* user_media_request) {
  pending_device_requests_->CancelUserMediaRequest(user_media_request);
  pending_display_requests_->CancelUserMediaRequest(user_media_request);
}

void UserMediaClient::DeleteAllUserMediaRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (frame_)
    frame_->SetIsCapturingMediaCallback(LocalFrame::IsCapturingMediaCallback());
  pending_device_requests_->DeleteAllUserMediaRequests();
  pending_display_requests_->DeleteAllUserMediaRequests();
}

void UserMediaClient::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Cancel all outstanding UserMediaRequests.
  DeleteAllUserMediaRequests();
}

void UserMediaClient::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(frame_);
  visitor->Trace(media_devices_dispatcher_);
  visitor->Trace(pending_device_requests_);
  visitor->Trace(pending_display_requests_);
}

void UserMediaClient::SetMediaDevicesDispatcherForTesting(
    mojo::PendingRemote<blink::mojom::blink::MediaDevicesDispatcherHost>
        media_devices_dispatcher) {
  media_devices_dispatcher_.Bind(
      std::move(media_devices_dispatcher),
      frame_->GetTaskRunner(blink::TaskType::kInternalMedia));
}

blink::mojom::blink::MediaDevicesDispatcherHost*
UserMediaClient::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_.is_bound()) {
    frame_->GetBrowserInterfaceBroker().GetInterface(
        media_devices_dispatcher_.BindNewPipeAndPassReceiver(
            frame_->GetTaskRunner(blink::TaskType::kInternalMedia)));
  }

  return media_devices_dispatcher_.get();
}

const char UserMediaClient::kSupplementName[] = "UserMediaClient";

UserMediaClient* UserMediaClient::From(LocalDOMWindow* window) {
  if (!window) {
    return nullptr;
  }
  auto* client = Supplement<LocalDOMWindow>::From<UserMediaClient>(window);
  if (!client) {
    if (!window->GetFrame()) {
      return nullptr;
    }
    client = MakeGarbageCollected<UserMediaClient>(
        window->GetFrame(), window->GetTaskRunner(TaskType::kInternalMedia));
    Supplement<LocalDOMWindow>::ProvideTo(*window, client);
  }
  return client;
}

void UserMediaClient::KeepDeviceAliveForTransfer(
    base::UnguessableToken session_id,
    base::UnguessableToken transfer_id,
    UserMediaProcessor::KeepDeviceAliveForTransferCallback keep_alive_cb) {
  pending_display_requests_->KeepDeviceAliveForTransfer(
      session_id, transfer_id, std::move(keep_alive_cb));
}

UserMediaClient::RequestQueue* UserMediaClient::GetRequestQueue(
    mojom::blink::MediaStreamType media_stream_type) {
  if (IsScreenCaptureMediaType(media_stream_type) ||
      media_stream_type ==
          mojom::blink::MediaStreamType::DISPLAY_AUDIO_CAPTURE) {
    return pending_display_requests_.Get();
  } else {
    return pending_device_requests_.Get();
  }
}

}  // namespace blink
