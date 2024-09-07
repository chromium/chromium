// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/picture_in_picture_controller_impl.h"

#include <limits>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/media/display_type.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if !BUILDFLAG(TARGET_OS_IS_ANDROID)
#include "third_party/blink/public/web/web_picture_in_picture_window_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_document_picture_in_picture_options.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture.h"
#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture_event.h"
#endif  // !BUILDFLAG(TARGET_OS_IS_ANDROID)

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

  // Picture-in-Picture is not allowed if the window is a document
  // Picture-in-Picture window.
  if (RuntimeEnabledFeatures::DocumentPictureInPictureAPIEnabled(
          GetSupplementable()->GetExecutionContext()) &&
      DomWindow() && DomWindow()->IsPictureInPictureWindow()) {
    return Status::kDocumentPip;
  }

  // `GetPictureInPictureEnabled()` returns false when the embedder or the
  // system forbids the page from using Picture-in-Picture.
  DCHECK(GetSupplementable()->GetSettings());
  if (!GetSupplementable()->GetSettings()->GetPictureInPictureEnabled())
    return Status::kDisabledBySystem;

  // If document is not allowed to use the policy-controlled feature named
  // "picture-in-picture", return kDisabledByPermissionsPolicy status.
  if (!GetSupplementable()->GetExecutionContext()->IsFeatureEnabled(
          blink::mojom::blink::PermissionsPolicyFeature::kPictureInPicture,
          report_failure ? ReportOptions::kReportOnFailure
                         : ReportOptions::kDoNotReport)) {
    return Status::kDisabledByPermissionsPolicy;
  }

  return Status::kEnabled;
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

  if (video_element.IsInAutoPIP())
    return Status::kAutoPipAndroid;

  return Status::kEnabled;
}

void PictureInPictureControllerImpl::EnterPictureInPicture(
    HTMLVideoElement* video_element,
    ScriptPromiseResolver<PictureInPictureWindow>* resolver) {
  if (!video_element->GetWebMediaPlayer()) {
    if (resolver) {
      // TODO(crbug.com/1293949): Add an error message.
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       "");
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

  gfx::Rect video_bounds;
  if (auto* layout_video =
          DynamicTo<LayoutVideo>(video_element->GetLayoutObject())) {
    PhysicalRect content_rect = layout_video->ReplacedContentRect();
    video_bounds = video_element->GetDocument().View()->FrameToViewport(
        ToEnclosingRect(layout_video->LocalToAbsoluteRect(content_rect)));
  } else {
    video_bounds = video_element->BoundsInWidget();
  }

  picture_in_picture_service_->StartSession(
      video_element->GetWebMediaPlayer()->GetDelegateId(),
      std::move(media_player_remote),
      video_element->GetWebMediaPlayer()->GetSurfaceId().value(),
      video_element->GetWebMediaPlayer()->NaturalSize(),
      ShouldShowPlayPauseButton(*video_element), std::move(session_observer),
      video_bounds,
      WTF::BindOnce(&PictureInPictureControllerImpl::OnEnteredPictureInPicture,
                    WrapPersistent(this), WrapPersistent(video_element),
                    WrapPersistent(resolver)));
}

void PictureInPictureControllerImpl::OnEnteredPictureInPicture(
    HTMLVideoElement* element,
    ScriptPromiseResolver<PictureInPictureWindow>* resolver,
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
      resolver->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                       "Picture-in-Picture is not available.");
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
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       "");
    }

    ExitPictureInPicture(element, nullptr);
    return;
  }

  if (picture_in_picture_element_)
    OnExitedPictureInPicture(nullptr);

#if !BUILDFLAG(TARGET_OS_IS_ANDROID)
  if (document_picture_in_picture_window_) {
    // TODO(crbug.com/1360452): close the window too.
    document_picture_in_picture_window_ = nullptr;
  }
#endif  // !BUILDFLAG(TARGET_OS_IS_ANDROID)

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
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  if (!EnsureService())
    return;

  if (!picture_in_picture_session_.is_bound())
    return;

  picture_in_picture_session_->Stop(
      WTF::BindOnce(&PictureInPictureControllerImpl::OnExitedPictureInPicture,
                    WrapPersistent(this), WrapPersistent(resolver)));
  session_observer_receiver_.reset();
}

void PictureInPictureControllerImpl::OnExitedPictureInPicture(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
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

    picture_in_picture_window_ = nullptr;

    // Register the video frame sink back to the element when the PiP window
    // is closed and if the video is not unset.
    if (element->GetWebMediaPlayer()) {
      element->GetWebMediaPlayer()->RegisterFrameSinkHierarchy();
    }
  }

  if (resolver)
    resolver->Resolve();
}

PictureInPictureWindow* PictureInPictureControllerImpl::pictureInPictureWindow()
    const {
  return picture_in_picture_window_.Get();
}

Element* PictureInPictureControllerImpl::PictureInPictureElement() const {
  return picture_in_picture_element_.Get();
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

#if !BUILDFLAG(TARGET_OS_IS_ANDROID)
LocalDOMWindow* PictureInPictureControllerImpl::documentPictureInPictureWindow()
    const {
  return document_picture_in_picture_window_.Get();
}

LocalDOMWindow*
PictureInPictureControllerImpl::GetDocumentPictureInPictureWindow() const {
  return document_picture_in_picture_window_;
}

LocalDOMWindow*
PictureInPictureControllerImpl::GetDocumentPictureInPictureOwner() const {
  return document_picture_in_picture_owner_;
}

void PictureInPictureControllerImpl::SetDocumentPictureInPictureOwner(
    LocalDOMWindow* owner) {
  CHECK(owner);

  document_picture_in_picture_owner_ = owner;
  document_pip_context_observer_ =
      MakeGarbageCollected<DocumentPictureInPictureObserver>(this);
  document_pip_context_observer_->SetContextLifecycleNotifier(owner);
}

void PictureInPictureControllerImpl::CreateDocumentPictureInPictureWindow(
    ScriptState* script_state,
    LocalDOMWindow& opener,
    DocumentPictureInPictureOptions* options,
    ScriptPromiseResolver<DOMWindow>* resolver) {
  if (!LocalFrame::ConsumeTransientUserActivation(opener.GetFrame())) {
    resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                     "Document PiP requires user activation");
    return;
  }

  WebPictureInPictureWindowOptions web_options;
  web_options.width = options->width();
  web_options.height = options->height();
  web_options.disallow_return_to_opener = options->disallowReturnToOpener();
  web_options.prefer_initial_window_placement =
      options->preferInitialWindowPlacement();

  // If either width or height is specified, then both must be specified.
  if (web_options.width > 0 && web_options.height == 0) {
    resolver->RejectWithRangeError(
        "Height must be specified if width is specified");
    return;
  } else if (web_options.width == 0 && web_options.height > 0) {
    resolver->RejectWithRangeError(
        "Width must be specified if height is specified");
    return;
  }

  auto* dom_window = opener.openPictureInPictureWindow(
      script_state->GetIsolate(), web_options);

  if (!dom_window) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Internal error: no window");
    return;
  }

  auto* local_dom_window = dom_window->ToLocalDOMWindow();
  DCHECK(local_dom_window);

  // Instantiate WindowProxy, so that a script state can be created for it
  // successfully later.
  // TODO(https://crbug.com/1336142): This should not be necessary.
  local_dom_window->GetScriptController().WindowProxy(script_state->World());

  // Set the Picture-in-Picture window's base URL to be the same as the opener
  // window's so that relative URLs will be resolved in the same way.
  Document* pip_document = local_dom_window->document();
  DCHECK(pip_document);
  pip_document->SetBaseURLOverride(opener.document()->BaseURL());

  SetMayThrottleIfUndrawnFrames(false);

  if (!document_pip_context_observer_) {
    document_pip_context_observer_ =
        MakeGarbageCollected<DocumentPictureInPictureObserver>(this);
  }
  document_pip_context_observer_->SetContextLifecycleNotifier(
      pip_document->GetExecutionContext());

  // While this API could be synchronous since we're using the |window.open()|
  // API to open the PiP window, we still use a Promise and post a task to make
  // it asynchronous because:
  // 1) We may eventually make this an asynchronous call to the browsser
  // 2) Other UAs may want to implement the API in an asynchronous way

  // If we have a task waiting already, just cancel the task and immediately
  // resolve.
  if (open_document_pip_task_.IsActive()) {
    open_document_pip_task_.Cancel();
    ResolveOpenDocumentPictureInPicture();
  }

  document_picture_in_picture_window_ = local_dom_window;

  // Give the pip document's PictureInPictureControllerImpl a pointer to our
  // window as its owner/opener.
  From(*pip_document)
      .SetDocumentPictureInPictureOwner(GetSupplementable()->domWindow());

  // There should not be an unresolved ScriptPromiseResolverBase at this point.
  // Leaving one unresolved and letting it get garbage collected will crash the
  // renderer.
  DCHECK(!open_document_pip_resolver_);
  open_document_pip_resolver_ = resolver;

  open_document_pip_task_ = PostCancellableTask(
      *opener.GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
      WTF::BindOnce(
          &PictureInPictureControllerImpl::ResolveOpenDocumentPictureInPicture,
          WrapPersistent(this)));
}

void PictureInPictureControllerImpl::ResolveOpenDocumentPictureInPicture() {
  CHECK(document_picture_in_picture_window_);
  CHECK(open_document_pip_resolver_);

  if (DomWindow()) {
    DocumentPictureInPicture::From(*DomWindow())
        ->DispatchEvent(*DocumentPictureInPictureEvent::Create(
            event_type_names::kEnter,
            WrapPersistent(document_picture_in_picture_window_.Get())));
  }

  open_document_pip_resolver_->Resolve(document_picture_in_picture_window_);
  open_document_pip_resolver_ = nullptr;
}

PictureInPictureControllerImpl::DocumentPictureInPictureObserver::
    DocumentPictureInPictureObserver(PictureInPictureControllerImpl* controller)
    : controller_(controller) {}
PictureInPictureControllerImpl::DocumentPictureInPictureObserver::
    ~DocumentPictureInPictureObserver() = default;

void PictureInPictureControllerImpl::DocumentPictureInPictureObserver::
    ContextDestroyed() {
  controller_->OnDocumentPictureInPictureContextDestroyed();
}

void PictureInPictureControllerImpl::DocumentPictureInPictureObserver::Trace(
    Visitor* visitor) const {
  visitor->Trace(controller_);
  ContextLifecycleObserver::Trace(visitor);
}

void PictureInPictureControllerImpl::
    OnDocumentPictureInPictureContextDestroyed() {
  // If we have an owner, then we are contained in a picture-in-picture window
  // and our owner's context has been destroyed.
  if (document_picture_in_picture_owner_) {
    CHECK(!document_picture_in_picture_window_);
    OnDocumentPictureInPictureOwnerWindowContextDestroyed();
    return;
  }

  // Otherwise, our owned picture-in-picture window's context has been
  // destroyed.
  OnOwnedDocumentPictureInPictureWindowContextDestroyed();
}

void PictureInPictureControllerImpl::
    OnOwnedDocumentPictureInPictureWindowContextDestroyed() {
  // The document PIP window has been destroyed, so the opener is no longer
  // associated with it.  Allow throttling again.
  SetMayThrottleIfUndrawnFrames(true);
  document_picture_in_picture_window_ = nullptr;

  // If there is an unresolved promise for a document PiP window, reject it now.
  // Note that we know that it goes with the current session, since we replace
  // the context observer's context at the same time we replace the session.
  if (open_document_pip_task_.IsActive()) {
    open_document_pip_task_.Cancel();
    open_document_pip_resolver_->Reject();
    open_document_pip_resolver_ = nullptr;
  }
}

void PictureInPictureControllerImpl::
    OnDocumentPictureInPictureOwnerWindowContextDestroyed() {
  document_picture_in_picture_owner_ = nullptr;
}
#endif  // !BUILDFLAG(TARGET_OS_IS_ANDROID)

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
  if (!GetSupplementable()->GetFrame() ||
      !GetSupplementable()->GetFrame()->GetWidgetForLocalRoot()) {
    // Tests do not always have a frame or widget.
    return;
  }
  GetSupplementable()
      ->GetFrame()
      ->GetWidgetForLocalRoot()
      ->SetMayThrottleIfUndrawnFrames(may_throttle);
}

void PictureInPictureControllerImpl::Trace(Visitor* visitor) const {
#if !BUILDFLAG(TARGET_OS_IS_ANDROID)
  visitor->Trace(document_picture_in_picture_window_);
  visitor->Trace(document_picture_in_picture_owner_);
  visitor->Trace(document_pip_context_observer_);
  visitor->Trace(open_document_pip_resolver_);
#endif  // !BUILDFLAG(TARGET_OS_IS_ANDROID)
  visitor->Trace(picture_in_picture_element_);
  visitor->Trace(picture_in_picture_window_);
  visitor->Trace(session_observer_receiver_);
  visitor->Trace(picture_in_picture_service_);
  visitor->Trace(picture_in_picture_session_);
  PictureInPictureController::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

PictureInPictureControllerImpl::PictureInPictureControllerImpl(
    Document& document)
    : PictureInPictureController(document),
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
