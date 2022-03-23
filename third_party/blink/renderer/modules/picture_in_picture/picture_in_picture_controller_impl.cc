// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"

#include <limits>
#include <utility>

#include "base/callback_helpers.h"
#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/web/web_picture_in_picture_window_options.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_picture_in_picture_window_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_event.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

bool ShouldShowPlayPauseButton(const HTMLVideoElement& element) {
  return element.GetLoadType() != WebMediaPlayer::kLoadTypeMediaStream &&
         element.duration() != std::numeric_limits<double>::infinity();
}

}  // namespace

// static
PictureInPictureControllerImpl& PictureInPictureControllerImpl::From(
    Document& document) {
  return static_cast<PictureInPictureControllerImpl&>(
      PictureInPictureController::From(document));
}

bool PictureInPictureControllerImpl::PictureInPictureEnabled() const {
  return IsDocumentAllowed(/*report_failure=*/true) == Status::kEnabled;
}

PictureInPictureController::Status
PictureInPictureControllerImpl::IsDocumentAllowed(bool report_failure) const {
  DCHECK(GetSupplementable());

  // If document has been detached from a frame, return kFrameDetached status.
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame)
    return Status::kFrameDetached;

  // `GetPictureInPictureEnabled()` returns false when the embedder or the
  // system forbids the page from using Picture-in-Picture.
  DCHECK(GetSupplementable()->GetSettings());
  if (!GetSupplementable()->GetSettings()->GetPictureInPictureEnabled())
    return Status::kDisabledBySystem;

  // If document is not allowed to use the policy-controlled feature named
  // "picture-in-picture", return kDisabledByPermissionsPolicy status.
  if (RuntimeEnabledFeatures::PictureInPictureAPIEnabled() &&
      !GetSupplementable()->GetExecutionContext()->IsFeatureEnabled(
          blink::mojom::blink::PermissionsPolicyFeature::kPictureInPicture,
          report_failure ? ReportOptions::kReportOnFailure
                         : ReportOptions::kDoNotReport)) {
    return Status::kDisabledByPermissionsPolicy;
  }

  return Status::kEnabled;
}

PictureInPictureController::Status
PictureInPictureControllerImpl::IsElementAllowed(
    const HTMLVideoElement& element) const {
  return IsElementAllowed(element, /*report_failure=*/false);
}

PictureInPictureController::Status
PictureInPictureControllerImpl::IsElementAllowed(
    const HTMLVideoElement& video_element,
    bool report_failure) const {
  PictureInPictureController::Status status = IsDocumentAllowed(report_failure);
  if (status != Status::kEnabled)
    return status;

  if (video_element.getReadyState() == HTMLMediaElement::kHaveNothing)
    return Status::kMetadataNotLoaded;

  if (!video_element.HasVideo())
    return Status::kVideoTrackNotAvailable;

  if (video_element.FastHasAttribute(html_names::kDisablepictureinpictureAttr))
    return Status::kDisabledByAttribute;

  return Status::kEnabled;
}

void PictureInPictureControllerImpl::EnterPictureInPicture(
    HTMLVideoElement* video_element,
    ScriptPromiseResolver* resolver) {
  if (!video_element->GetWebMediaPlayer()) {
    if (resolver) {
      // TODO(crbug.com/1293949): Add an error message.
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kInvalidStateError, ""));
    }

    return;
  }

  if (picture_in_picture_element_ == video_element) {
    if (resolver)
      resolver->Resolve(picture_in_picture_window_);

    return;
  }

  if (!EnsureService())
    return;

  if (video_element->GetDisplayType() == DisplayType::kFullscreen)
    Fullscreen::ExitFullscreen(*GetSupplementable());

  video_element->GetWebMediaPlayer()->OnRequestPictureInPicture();
  DCHECK(video_element->GetWebMediaPlayer()->GetSurfaceId().has_value());

  session_observer_receiver_.reset();

  mojo::PendingRemote<mojom::blink::PictureInPictureSessionObserver>
      session_observer;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      video_element->GetDocument().GetTaskRunner(TaskType::kMediaElementEvent);
  session_observer_receiver_.Bind(
      session_observer.InitWithNewPipeAndPassReceiver(), task_runner);

  mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>
      media_player_remote;
  video_element->BindMediaPlayerReceiver(
      media_player_remote.InitWithNewEndpointAndPassReceiver());

  picture_in_picture_service_->StartSession(
      video_element->GetWebMediaPlayer()->GetDelegateId(),
      std::move(media_player_remote),
      video_element->GetWebMediaPlayer()->GetSurfaceId().value(),
      video_element->GetWebMediaPlayer()->NaturalSize(),
      ShouldShowPlayPauseButton(*video_element), std::move(session_observer),
      WTF::Bind(&PictureInPictureControllerImpl::OnEnteredPictureInPicture,
                WrapPersistent(this), WrapPersistent(video_element),
                WrapPersistent(resolver)));
}

void PictureInPictureControllerImpl::OnEnteredPictureInPicture(
    HTMLVideoElement* element,
    ScriptPromiseResolver* resolver,
    mojo::PendingRemote<mojom::blink::PictureInPictureSession> session_remote,
    const gfx::Size& picture_in_picture_window_size) {
  // If |session_ptr| is null then Picture-in-Picture is not supported by the
  // browser. We should rarely see this because we should have already rejected
  // with |kDisabledBySystem|.
  if (!session_remote) {
    if (resolver &&
        IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                      resolver->GetScriptState())) {
      ScriptState::Scope script_state_scope(resolver->GetScriptState());
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kNotSupportedError,
          "Picture-in-Picture is not available."));
    }

    return;
  }

  picture_in_picture_session_.reset();
  picture_in_picture_session_.Bind(
      std::move(session_remote),
      element->GetDocument().GetTaskRunner(TaskType::kMediaElementEvent));
  if (IsElementAllowed(*element, /*report_failure=*/true) != Status::kEnabled) {
    if (resolver &&
        IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                      resolver->GetScriptState())) {
      ScriptState::Scope script_state_scope(resolver->GetScriptState());
      // TODO(crbug.com/1293949): Add an error message.
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kInvalidStateError, ""));
    }

    ExitPictureInPicture(element, nullptr);
    return;
  }

  if (picture_in_picture_element_)
    OnExitedPictureInPicture(nullptr);

  picture_in_picture_element_ = element;
  picture_in_picture_element_->OnEnteredPictureInPicture();

  // Request that viz does not throttle our LayerTree's BeginFrame messages, in
  // case this page generates them as a side-effect of driving picture-in-
  // picture content.  See the header file for more details, or
  // https://crbug.com/1232173
  SetMayThrottleIfUndrawnFrames(false);

  picture_in_picture_window_ = MakeGarbageCollected<PictureInPictureWindow>(
      GetExecutionContext(), picture_in_picture_window_size);

  picture_in_picture_element_->DispatchEvent(*PictureInPictureEvent::Create(
      event_type_names::kEnterpictureinpicture,
      WrapPersistent(picture_in_picture_window_.Get())));

  if (resolver)
    resolver->Resolve(picture_in_picture_window_);

  // Unregister the video frame sink from the element since it will be moved
  // to be the child of the PiP window frame sink.
  if (picture_in_picture_element_->GetWebMediaPlayer()) {
    picture_in_picture_element_->GetWebMediaPlayer()
        ->UnregisterFrameSinkHierarchy();
  }
}

void PictureInPictureControllerImpl::ExitPictureInPicture(
    HTMLVideoElement* element,
    ScriptPromiseResolver* resolver) {
  if (!EnsureService())
    return;

  if (!picture_in_picture_session_.is_bound())
    return;

  picture_in_picture_session_->Stop(
      WTF::Bind(&PictureInPictureControllerImpl::OnExitedPictureInPicture,
                WrapPersistent(this), WrapPersistent(resolver)));
  session_observer_receiver_.reset();
}

void PictureInPictureControllerImpl::OnExitedPictureInPicture(
    ScriptPromiseResolver* resolver) {
  DCHECK(GetSupplementable());

  // Bail out if document is not active.
  if (!GetSupplementable()->IsActive())
    return;

  // Now that this widget is not responsible for providing the content for a
  // Picture in Picture window, we should not be producing CompositorFrames
  // while the widget is hidden.  Let viz know that throttling us is okay if we
  // do that.
  SetMayThrottleIfUndrawnFrames(true);

  // The Picture-in-Picture window and the Picture-in-Picture element
  // should be either both set or both null.
  DCHECK(!picture_in_picture_element_ == !picture_in_picture_window_);
  if (picture_in_picture_element_) {
    picture_in_picture_window_->OnClose();

    HTMLVideoElement* element = picture_in_picture_element_;
    picture_in_picture_element_ = nullptr;

    element->OnExitedPictureInPicture();
    element->DispatchEvent(*PictureInPictureEvent::Create(
        event_type_names::kLeavepictureinpicture,
        WrapPersistent(picture_in_picture_window_.Get())));

    // Register the video frame sink back to the element when the PiP window
    // is closed and if the video is not unset.
    if (element->GetWebMediaPlayer()) {
      element->GetWebMediaPlayer()->RegisterFrameSinkHierarchy();
    }
  }

  if (resolver)
    resolver->Resolve();
}

Element* PictureInPictureControllerImpl::PictureInPictureElement() const {
  return picture_in_picture_element_;
}

Element* PictureInPictureControllerImpl::PictureInPictureElement(
    TreeScope& scope) const {
  if (!picture_in_picture_element_)
    return nullptr;

  return scope.AdjustedElement(*picture_in_picture_element_);
}

bool PictureInPictureControllerImpl::IsPictureInPictureElement(
    const Element* element) const {
  DCHECK(element);
  return element == picture_in_picture_element_;
}

void PictureInPictureControllerImpl::AddToAutoPictureInPictureElementsList(
    HTMLVideoElement* element) {
  RemoveFromAutoPictureInPictureElementsList(element);
  auto_picture_in_picture_elements_.push_back(element);
}

void PictureInPictureControllerImpl::RemoveFromAutoPictureInPictureElementsList(
    HTMLVideoElement* element) {
  DCHECK(element);
  auto it = std::find(auto_picture_in_picture_elements_.begin(),
                      auto_picture_in_picture_elements_.end(), element);
  if (it != auto_picture_in_picture_elements_.end())
    auto_picture_in_picture_elements_.erase(it);
}

HTMLVideoElement* PictureInPictureControllerImpl::AutoPictureInPictureElement()
    const {
  return auto_picture_in_picture_elements_.IsEmpty()
             ? nullptr
             : auto_picture_in_picture_elements_.back();
}

bool PictureInPictureControllerImpl::IsEnterAutoPictureInPictureAllowed()
    const {
  // Entering Auto Picture-in-Picture is allowed if one of these conditions is
  // true:
  // - Document runs in a Chrome Extension.
  // - Document is in fullscreen.
  // - Document is in a PWA window that runs in the scope of the PWA.
  bool is_in_pwa_window = false;
  if (GetSupplementable()->GetFrame()) {
    mojom::blink::DisplayMode display_mode =
        GetSupplementable()->GetFrame()->GetWidgetForLocalRoot()->DisplayMode();
    is_in_pwa_window = display_mode != mojom::blink::DisplayMode::kBrowser;
  }
  if (!(CommonSchemeRegistry::IsExtensionScheme(
            GetSupplementable()->Url().Protocol().Ascii()) ||
        Fullscreen::FullscreenElementFrom(*GetSupplementable()) ||
        (is_in_pwa_window && GetSupplementable()->IsInWebAppScope()))) {
    return false;
  }

  // Don't allow if there's already an element in Auto Picture-in-Picture.
  if (picture_in_picture_element_)
    return false;

  // Don't allow if there's no element eligible to enter Auto Picture-in-Picture
  if (!AutoPictureInPictureElement())
    return false;

  // Don't allow if video won't resume playing automatically when it becomes
  // visible again.
  if (AutoPictureInPictureElement()->PausedWhenVisible())
    return false;

  // Allow if video is allowed to enter Picture-in-Picture.
  return (IsElementAllowed(*AutoPictureInPictureElement(),
                           /*report_failure=*/true) == Status::kEnabled);
}

bool PictureInPictureControllerImpl::IsExitAutoPictureInPictureAllowed() const {
  // Don't allow exiting Auto Picture-in-Picture if there's no eligible element
  // to exit Auto Picture-in-Picture.
  if (!AutoPictureInPictureElement())
    return false;

  // Allow if the element already in Picture-in-Picture is the same as the one
  // eligible to exit Auto Picture-in-Picture.
  return (picture_in_picture_element_ == AutoPictureInPictureElement());
}

void PictureInPictureControllerImpl::CreateDocumentPictureInPictureWindow(
    ScriptState* script_state,
    LocalDOMWindow& opener,
    PictureInPictureWindowOptions* options,
    ScriptPromiseResolver* resolver,
    ExceptionState& exception_state) {
  WebPictureInPictureWindowOptions web_options;
  web_options.size = gfx::Size(options->width(), options->height());
  web_options.constrain_aspect_ratio = options->constrainAspectRatio();

  auto* dom_window = opener.openPictureInPictureWindow(
      script_state->GetIsolate(), web_options, exception_state);

  // If we can't create a window then reject the promise with the exception
  // state.
  if (!dom_window || exception_state.HadException()) {
    resolver->Reject();
    return;
  }

  auto* local_dom_window = dom_window->ToLocalDOMWindow();
  DCHECK(local_dom_window);

  // TODO(https://crbug.com/1253970): Use the real size returned by the browser
  // side when we get one.
  picture_in_picture_window_ = MakeGarbageCollected<PictureInPictureWindow>(
      GetExecutionContext(), web_options.size, local_dom_window->document());

  resolver->Resolve(picture_in_picture_window_);
}

void PictureInPictureControllerImpl::PageVisibilityChanged() {
  DCHECK(GetSupplementable());

  // If page becomes visible and exiting Auto Picture-in-Picture is allowed,
  // exit Picture-in-Picture.
  if (GetSupplementable()->IsPageVisible() &&
      IsExitAutoPictureInPictureAllowed()) {
    ExitPictureInPicture(picture_in_picture_element_, nullptr);
    return;
  }

  // If page becomes hidden and entering Auto Picture-in-Picture is allowed,
  // enter Picture-in-Picture.
  if (GetSupplementable()->hidden() && IsEnterAutoPictureInPictureAllowed()) {
    EnterPictureInPicture(AutoPictureInPictureElement(), /*promise=*/nullptr);
  }
}

void PictureInPictureControllerImpl::OnPictureInPictureStateChange() {
  DCHECK(picture_in_picture_element_);
  DCHECK(picture_in_picture_element_->GetWebMediaPlayer());
  DCHECK(picture_in_picture_element_->GetWebMediaPlayer()
             ->GetSurfaceId()
             .has_value());

  // The lifetime of the MediaPlayer mojo endpoint in the renderer is tied to
  // WebMediaPlayer, which is recreated by |picture_in_picture_element_| on
  // src= change. Since src= change is one of the reasons we get here, we need
  // to give the browser a newly bound remote.
  mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>
      media_player_remote;
  picture_in_picture_element_->BindMediaPlayerReceiver(
      media_player_remote.InitWithNewEndpointAndPassReceiver());

  picture_in_picture_session_->Update(
      picture_in_picture_element_->GetWebMediaPlayer()->GetDelegateId(),
      std::move(media_player_remote),
      picture_in_picture_element_->GetWebMediaPlayer()->GetSurfaceId().value(),
      picture_in_picture_element_->GetWebMediaPlayer()->NaturalSize(),
      ShouldShowPlayPauseButton(*picture_in_picture_element_));
}

void PictureInPictureControllerImpl::OnWindowSizeChanged(
    const gfx::Size& size) {
  if (picture_in_picture_window_)
    picture_in_picture_window_->OnResize(size);
}

void PictureInPictureControllerImpl::OnStopped() {
  OnExitedPictureInPicture(nullptr);
}

void PictureInPictureControllerImpl::SetMayThrottleIfUndrawnFrames(
    bool may_throttle) {
  GetSupplementable()
      ->GetFrame()
      ->GetWidgetForLocalRoot()
      ->SetMayThrottleIfUndrawnFrames(may_throttle);
}

void PictureInPictureControllerImpl::Trace(Visitor* visitor) const {
  visitor->Trace(picture_in_picture_element_);
  visitor->Trace(auto_picture_in_picture_elements_);
  visitor->Trace(picture_in_picture_window_);
  visitor->Trace(session_observer_receiver_);
  visitor->Trace(picture_in_picture_service_);
  visitor->Trace(picture_in_picture_session_);
  PictureInPictureController::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

PictureInPictureControllerImpl::PictureInPictureControllerImpl(
    Document& document)
    : PictureInPictureController(document),
      PageVisibilityObserver(document.GetPage()),
      ExecutionContextClient(document.GetExecutionContext()),
      session_observer_receiver_(this, document.GetExecutionContext()),
      picture_in_picture_service_(document.GetExecutionContext()),
      picture_in_picture_session_(document.GetExecutionContext()) {}

bool PictureInPictureControllerImpl::EnsureService() {
  if (picture_in_picture_service_.is_bound())
    return true;

  if (!GetSupplementable()->GetFrame())
    return false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetSupplementable()->GetFrame()->GetTaskRunner(
          TaskType::kMediaElementEvent);
  GetSupplementable()->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      picture_in_picture_service_.BindNewPipeAndPassReceiver(task_runner));
  return true;
}

}  // namespace blink
