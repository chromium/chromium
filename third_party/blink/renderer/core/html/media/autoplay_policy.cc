// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/autoplay_uma_helper.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

const char kWarningUnmuteFailed[] =
    "Unmuting failed and the element was paused instead because the user "
    "didn't interact with the document before. https://goo.gl/xX8pDD";
const char kErrorAutoplayFuncUnified[] =
    "play() failed because the user didn't interact with the document first. "
    "https://goo.gl/xX8pDD";
const char kErrorAutoplayFuncMobile[] =
    "play() can only be initiated by a user gesture.";

// Return true if and only if the document settings specifies media playback
// requires user gesture on the element.
bool ComputeLockPendingUserGestureRequired(const Document& document) {
  switch (AutoplayPolicy::GetAutoplayPolicyForDocument(document)) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      return false;
    case AutoplayPolicy::Type::kUserGestureRequired:
      return true;
    // kDocumentUserActivationRequired policy does not imply that a user gesture
    // is required on the element but instead requires a user gesture on the
    // document, therefore the element is not locked.
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return true;
}

}  // anonymous namespace

// static
AutoplayPolicy::Type AutoplayPolicy::GetAutoplayPolicyForDocument(
    const Document& document) {
  if (!document.GetSettings())
    return Type::kNoUserGestureRequired;

  if (document.IsInWebAppScope())
    return Type::kNoUserGestureRequired;

  if (DocumentHasUserExceptionFlag(document))
    return Type::kNoUserGestureRequired;

  if (document.GetSettings()->GetPresentationReceiver())
    return Type::kNoUserGestureRequired;

  return document.GetSettings()->GetAutoplayPolicy();
}

// static
bool AutoplayPolicy::IsDocumentAllowedToPlay(const Document& document) {
  if (DocumentHasForceAllowFlag(document))
    return true;

  if (DocumentIsCapturingUserMedia(document))
    return true;

  if (!document.GetFrame())
    return false;

  bool permissions_policy_enabled =
      document.GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kAutoplay);

  for (Frame* frame = document.GetFrame(); frame;
       frame = frame->Tree().Parent()) {
    if (frame->HasStickyUserActivation() ||
        frame->HadStickyUserActivationBeforeNavigation()) {
      return true;
    }

    if (RuntimeEnabledFeatures::
            MediaEngagementBypassAutoplayPoliciesEnabled() &&
        frame->IsOutermostMainFrame() &&
        DocumentHasHighMediaEngagement(document)) {
      return true;
    }

    if (!permissions_policy_enabled)
      return false;
  }

  return false;
}

// static
bool AutoplayPolicy::DocumentHasHighMediaEngagement(const Document& document) {
  if (!document.GetPage())
    return false;
  return document.GetPage()->AutoplayFlags() &
         mojom::blink::kAutoplayFlagHighMediaEngagement;
}

// static
bool AutoplayPolicy::DocumentHasForceAllowFlag(const Document& document) {
  if (!document.GetPage())
    return false;
  return document.GetPage()->AutoplayFlags() &
         mojom::blink::kAutoplayFlagForceAllow;
}

// static
bool AutoplayPolicy::DocumentHasUserExceptionFlag(const Document& document) {
  if (!document.GetPage())
    return false;
  return document.GetPage()->AutoplayFlags() &
         mojom::blink::kAutoplayFlagUserException;
}

// static
bool AutoplayPolicy::DocumentShouldAutoplayMutedVideos(
    const Document& document) {
  return GetAutoplayPolicyForDocument(document) !=
         AutoplayPolicy::Type::kNoUserGestureRequired;
}

// static
bool AutoplayPolicy::DocumentIsCapturingUserMedia(const Document& document) {
  if (auto* local_frame = document.GetFrame())
    return local_frame->IsCapturingMedia();

  return false;
}

AutoplayPolicy::AutoplayPolicy(HTMLMediaElement* element)
    : locked_pending_user_gesture_(false),
      element_(element),
      autoplay_uma_helper_(MakeGarbageCollected<AutoplayUmaHelper>(element)) {
  locked_pending_user_gesture_ =
      ComputeLockPendingUserGestureRequired(element->GetDocument());
}

void AutoplayPolicy::VideoWillBeDrawnToCanvas() const {
  autoplay_uma_helper_->VideoWillBeDrawnToCanvas();
}

void AutoplayPolicy::DidMoveToNewDocument(Document& old_document) {
  // If any experiment is enabled, then we want to enable a user gesture by
  // default, otherwise the experiment does nothing.
  bool old_document_requires_user_gesture =
      ComputeLockPendingUserGestureRequired(old_document);
  bool new_document_requires_user_gesture =
      ComputeLockPendingUserGestureRequired(element_->GetDocument());
  if (new_document_requires_user_gesture && !old_document_requires_user_gesture)
    locked_pending_user_gesture_ = true;

  autoplay_uma_helper_->DidMoveToNewDocument(old_document);
}

bool AutoplayPolicy::IsEligibleForAutoplayMuted() const {
  if (!IsA<HTMLVideoElement>(element_.Get()))
    return false;

  if (RuntimeEnabledFeatures::VideoAutoFullscreenEnabled() &&
      !element_->FastHasAttribute(html_names::kPlaysinlineAttr)) {
    return false;
  }

  return !element_->EffectiveMediaVolume() &&
         DocumentShouldAutoplayMutedVideos(element_->GetDocument());
}

void AutoplayPolicy::StartAutoplayMutedWhenVisible() {
  // We might end up in a situation where the previous
  // observer didn't had time to fire yet. We can avoid
  // creating a new one in this case.
  if (autoplay_intersection_observer_)
    return;

  autoplay_intersection_observer_ = IntersectionObserver::Create(
      element_->GetDocument(),
      WTF::BindRepeating(&AutoplayPolicy::OnIntersectionChangedForAutoplay,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kMediaIntersectionObserver,
      IntersectionObserver::Params{
          .thresholds = {IntersectionObserver::kMinimumThreshold}});
  autoplay_intersection_observer_->observe(element_);
}

void AutoplayPolicy::StopAutoplayMutedWhenVisible() {
  if (!autoplay_intersection_observer_)
    return;

  autoplay_intersection_observer_->disconnect();
  autoplay_intersection_observer_ = nullptr;
}

bool AutoplayPolicy::RequestAutoplayUnmute() {
  DCHECK_NE(0, element_->EffectiveMediaVolume());
  bool was_autoplaying_muted = IsAutoplayingMutedInternal(true);

  TryUnlockingUserGesture();

  if (was_autoplaying_muted) {
    if (IsGestureNeededForPlayback()) {
      if (IsUsingDocumentUserActivationRequiredPolicy()) {
        element_->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::ConsoleMessageSource::kJavaScript,
                mojom::ConsoleMessageLevel::kWarning, kWarningUnmuteFailed));
      }

      autoplay_uma_helper_->RecordAutoplayUnmuteStatus(
          AutoplayUnmuteActionStatus::kFailure);
      return false;
    }
    autoplay_uma_helper_->RecordAutoplayUnmuteStatus(
        AutoplayUnmuteActionStatus::kSuccess);
  }
  return true;
}

bool AutoplayPolicy::RequestAutoplayByAttribute() {
  if (!ShouldAutoplay())
    return false;

  autoplay_uma_helper_->OnAutoplayInitiated(AutoplaySource::kAttribute);

  if (IsGestureNeededForPlayback())
    return false;

  // If it's the first playback, track that it started because of autoplay.
  MaybeSetAutoplayInitiated();

  // At this point the gesture is not needed for playback per the if statement
  // above.
  if (!IsEligibleForAutoplayMuted())
    return true;

  // Autoplay muted video should be handled by AutoplayPolicy based on the
  // visibily.
  StartAutoplayMutedWhenVisible();
  return false;
}

bool AutoplayPolicy::HasTransientUserActivation() const {
  LocalFrame* frame = element_->GetDocument().GetFrame();
  if (!frame) {
    return false;
  }

  if (LocalFrame::HasTransientUserActivation(frame)) {
    return true;
  }

  Frame* opener = frame->Opener();
  if (opener && opener->IsLocalFrame() &&
      LocalFrame::HasTransientUserActivation(To<LocalFrame>(opener))) {
    return true;
  }

  return false;
}

std::optional<DOMExceptionCode> AutoplayPolicy::RequestPlay() {
  if (RuntimeEnabledFeatures::
          MediaPlaybackWhileNotVisiblePermissionPolicyEnabled() &&
      !CanPlayWhileHidden() && IsFrameHidden()) {
    return DOMExceptionCode::kNotAllowedError;
  }

  if (!HasTransientUserActivation()) {
    autoplay_uma_helper_->OnAutoplayInitiated(AutoplaySource::kMethod);
    if (IsGestureNeededForPlayback())
      return DOMExceptionCode::kNotAllowedError;
  } else {
    TryUnlockingUserGesture();
  }

  MaybeSetAutoplayInitiated();

  return std::nullopt;
}

bool AutoplayPolicy::IsAutoplayingMutedInternal(bool muted) const {
  return !element_->paused() && IsOrWillBeAutoplayingMutedInternal(muted);
}

bool AutoplayPolicy::IsOrWillBeAutoplayingMuted() const {
  return IsOrWillBeAutoplayingMutedInternal(!element_->EffectiveMediaVolume());
}

bool AutoplayPolicy::IsOrWillBeAutoplayingMutedInternal(bool muted) const {
  if (!IsA<HTMLVideoElement>(element_.Get()) ||
      !DocumentShouldAutoplayMutedVideos(element_->GetDocument())) {
    return false;
  }

  return muted && IsLockedPendingUserGesture();
}

bool AutoplayPolicy::IsLockedPendingUserGesture() const {
  if (IsUsingDocumentUserActivationRequiredPolicy())
    return !IsDocumentAllowedToPlay(element_->GetDocument());

  return locked_pending_user_gesture_;
}

void AutoplayPolicy::TryUnlockingUserGesture() {
  if (IsLockedPendingUserGesture() && LocalFrame::HasTransientUserActivation(
                                          element_->GetDocument().GetFrame())) {
    locked_pending_user_gesture_ = false;
  }
}

bool AutoplayPolicy::IsGestureNeededForPlayback() const {
  if (!IsLockedPendingUserGesture())
    return false;

  // We want to allow muted video to autoplay if the element is allowed to
  // autoplay muted.
  return !IsEligibleForAutoplayMuted();
}

bool AutoplayPolicy::CanPlayWhileHidden() const {
  return element_->GetExecutionContext() &&
         element_->GetExecutionContext()->IsFeatureEnabled(
             mojom::blink::PermissionsPolicyFeature::
                 kMediaPlaybackWhileNotVisible);
}

bool AutoplayPolicy::IsFrameHidden() const {
  Frame* frame = element_->GetDocument().GetFrame();
  return frame && (frame->View()->GetFrameVisibility().value_or(
                       mojom::blink::FrameVisibility::kRenderedInViewport) ==
                   mojom::blink::FrameVisibility::kNotRendered);
}

String AutoplayPolicy::GetPlayErrorMessage() const {
  return IsUsingDocumentUserActivationRequiredPolicy()
             ? kErrorAutoplayFuncUnified
             : kErrorAutoplayFuncMobile;
}

bool AutoplayPolicy::WasAutoplayInitiated() const {
  if (!autoplay_initiated_.has_value())
    return false;

  return *autoplay_initiated_;
}

void AutoplayPolicy::EnsureAutoplayInitiatedSet() {
  if (autoplay_initiated_)
    return;
  autoplay_initiated_ = false;
}

void AutoplayPolicy::OnIntersectionChangedForAutoplay(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool is_visible = (entries.back()->intersectionRatio() > 0);

  if (!is_visible) {
    auto pause_and_preserve_autoplay = [](AutoplayPolicy* self) {
      if (!self)
        return;

      if (self->element_->can_autoplay_ && self->element_->Autoplay()) {
        self->element_->PauseInternal(
            HTMLMediaElement::PlayPromiseError::kPaused_AutoplayAutoPause);
        self->element_->can_autoplay_ = true;
      }
    };

    element_->GetDocument()
        .GetTaskRunner(TaskType::kInternalMedia)
        ->PostTask(FROM_HERE, WTF::BindOnce(pause_and_preserve_autoplay,
                                            WrapWeakPersistent(this)));
    return;
  }

  auto maybe_autoplay = [](AutoplayPolicy* self) {
    if (!self)
      return;

    if (self->ShouldAutoplay()) {
      self->element_->paused_ = false;
      self->element_->SetShowPosterFlag(false);
      self->element_->ScheduleNamedEvent(event_type_names::kPlay);
      self->element_->ScheduleNotifyPlaying();

      self->element_->UpdatePlayState();
    }
  };

  element_->GetDocument()
      .GetTaskRunner(TaskType::kInternalMedia)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(maybe_autoplay, WrapWeakPersistent(this)));
}

bool AutoplayPolicy::IsUsingDocumentUserActivationRequiredPolicy() const {
  return GetAutoplayPolicyForDocument(element_->GetDocument()) ==
         AutoplayPolicy::Type::kDocumentUserActivationRequired;
}

void AutoplayPolicy::MaybeSetAutoplayInitiated() {
  if (autoplay_initiated_.has_value())
    return;

  autoplay_initiated_ = true;

  bool permissions_policy_enabled =
      element_->GetExecutionContext() &&
      element_->GetExecutionContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kAutoplay);

  for (Frame* frame = element_->GetDocument().GetFrame(); frame;
       frame = frame->Tree().Parent()) {
    if (frame->HasStickyUserActivation() ||
        frame->HadStickyUserActivationBeforeNavigation()) {
      autoplay_initiated_ = false;
      break;
    }
    if (!permissions_policy_enabled)
      break;
  }
}

bool AutoplayPolicy::ShouldAutoplay() {
  if (!element_->GetExecutionContext() ||
      element_->GetExecutionContext()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kAutomaticFeatures)) {
    return false;
  }
  return element_->can_autoplay_ && element_->paused_ && element_->Autoplay();
}

void AutoplayPolicy::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(autoplay_intersection_observer_);
  visitor->Trace(autoplay_uma_helper_);
}

STATIC_ASSERT_ENUM(mojom::blink::AutoplayPolicy::kNoUserGestureRequired,
                   AutoplayPolicy::Type::kNoUserGestureRequired);
STATIC_ASSERT_ENUM(mojom::blink::AutoplayPolicy::kUserGestureRequired,
                   AutoplayPolicy::Type::kUserGestureRequired);
STATIC_ASSERT_ENUM(
    mojom::blink::AutoplayPolicy::kDocumentUserActivationRequired,
    AutoplayPolicy::Type::kDocumentUserActivationRequired);

}  // namespace blink
