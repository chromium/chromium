// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/token.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

#if !BUILDFLAG(IS_ANDROID)

using ApplySubCaptureTargetResult =
    BrowserCaptureMediaStreamTrack::ApplySubCaptureTargetResult;

// If crop_id is the empty string, returns an empty base::Token.
// If crop_id is a valid UUID, returns a base::Token representing the ID.
// Otherwise, returns nullopt.
std::optional<base::Token> IdStringToToken(const String& crop_id) {
  if (crop_id.empty()) {
    return base::Token();
  }
  if (!crop_id.ContainsOnlyASCIIOrEmpty()) {
    return std::nullopt;
  }
  const base::Uuid guid = base::Uuid::ParseCaseInsensitive(crop_id.Ascii());
  return guid.is_valid() ? std::make_optional<base::Token>(GUIDToToken(guid))
                         : std::nullopt;
}

void RaiseApplySubCaptureTargetException(
    ScriptPromiseResolverWithTracker<ApplySubCaptureTargetResult, IDLUndefined>*
        resolver,
    DOMExceptionCode exception_code,
    const WTF::String& exception_text,
    ApplySubCaptureTargetResult result) {
  resolver->Reject<DOMException>(
      MakeGarbageCollected<DOMException>(exception_code, exception_text),
      result);
}

void ResolveApplySubCaptureTargetPromiseHelper(
    ScriptPromiseResolverWithTracker<ApplySubCaptureTargetResult, IDLUndefined>*
        resolver,
    media::mojom::ApplySubCaptureTargetResult result) {
  DCHECK(IsMainThread());

  if (!resolver) {
    return;
  }

  switch (result) {
    case media::mojom::ApplySubCaptureTargetResult::kSuccess:
      resolver->Resolve();
      return;
    case media::mojom::ApplySubCaptureTargetResult::kErrorGeneric:
      RaiseApplySubCaptureTargetException(
          resolver, DOMExceptionCode::kAbortError, "Unknown error.",
          ApplySubCaptureTargetResult::kRejectedWithErrorGeneric);
      return;
    case media::mojom::ApplySubCaptureTargetResult::kUnsupportedCaptureDevice:
      // Note that this is an unsupported device; not an unsupported Element.
      // This should essentially not happen. If it happens, it indicates
      // something in the capture pipeline has been changed.
      RaiseApplySubCaptureTargetException(
          resolver, DOMExceptionCode::kAbortError, "Unsupported device.",
          ApplySubCaptureTargetResult::kRejectedWithUnsupportedCaptureDevice);
      return;
    case media::mojom::ApplySubCaptureTargetResult::kNotImplemented:
      // Unimplemented codepath reached, OTHER than lacking support for
      // a specific Element subtype.
      RaiseApplySubCaptureTargetException(
          resolver, DOMExceptionCode::kOperationError, "Not implemented.",
          ApplySubCaptureTargetResult::kRejectedWithNotImplemented);
      return;
    case media::mojom::ApplySubCaptureTargetResult::kNonIncreasingVersion:
      // This should rarely happen, as the browser process would issue
      // a BadMessage in this case. But if that message has to hop from
      // the IO thread to the UI thread, it could theoretically happen
      // that Blink receives this callback before being killed, so we
      // can't quite DCHECK this.
      RaiseApplySubCaptureTargetException(
          resolver, DOMExceptionCode::kAbortError, "Non-increasing version.",
          ApplySubCaptureTargetResult::kNonIncreasingVersion);
      return;
    case media::mojom::ApplySubCaptureTargetResult::kInvalidTarget:
      RaiseApplySubCaptureTargetException(
          resolver, DOMExceptionCode::kNotAllowedError, "Invalid target.",
          ApplySubCaptureTargetResult::kInvalidTarget);
      return;
  }

  NOTREACHED_IN_MIGRATION();
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    base::OnceClosure callback)
    : BrowserCaptureMediaStreamTrack(execution_context,
                                     component,
                                     component->GetReadyState(),
                                     std::move(callback)) {}

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    MediaStreamSource::ReadyState ready_state,
    base::OnceClosure callback)
    : MediaStreamTrackImpl(execution_context,
                           component,
                           ready_state,
                           std::move(callback)) {}

#if !BUILDFLAG(IS_ANDROID)
void BrowserCaptureMediaStreamTrack::Trace(Visitor* visitor) const {
  visitor->Trace(pending_promises_);
  MediaStreamTrackImpl::Trace(visitor);
}
#endif  // !BUILDFLAG(IS_ANDROID)

ScriptPromise<IDLUndefined> BrowserCaptureMediaStreamTrack::cropTo(
    ScriptState* script_state,
    CropTarget* target,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  return ApplySubCaptureTarget(script_state,
                               SubCaptureTarget::Type::kCropTarget, target,
                               exception_state);
}

ScriptPromise<IDLUndefined> BrowserCaptureMediaStreamTrack::restrictTo(
    ScriptState* script_state,
    RestrictionTarget* target,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  return ApplySubCaptureTarget(script_state,
                               SubCaptureTarget::Type::kRestrictionTarget,
                               target, exception_state);
}

BrowserCaptureMediaStreamTrack* BrowserCaptureMediaStreamTrack::clone(
    ExecutionContext* execution_context) {
  // Instantiate the clone.
  BrowserCaptureMediaStreamTrack* cloned_track =
      MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
          execution_context, Component()->Clone(), GetReadyState(),
          base::DoNothing());

  // Copy state.
  MediaStreamTrackImpl::CloneInternal(cloned_track);

  return cloned_track;
}

ScriptPromise<IDLUndefined>
BrowserCaptureMediaStreamTrack::ApplySubCaptureTarget(
    ScriptState* script_state,
    SubCaptureTarget::Type type,
    SubCaptureTarget* target,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  CHECK(type == SubCaptureTarget::Type::kCropTarget ||
        type == SubCaptureTarget::Type::kRestrictionTarget);

  const std::string metric_name_prefix =
      (type == SubCaptureTarget::Type::kCropTarget)
          ? "Media.RegionCapture.CropTo"
          : "Media.ElementCapture.RestrictTo";

  // If the promise is not resolved within the |timeout_interval|, an
  // ApplySubCaptureTargetResult::kTimedOut response will be recorded in the
  // UMA.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolverWithTracker<
      ApplySubCaptureTargetResult, IDLUndefined>>(
      script_state, metric_name_prefix,
      /*timeout_interval=*/base::Seconds(10));
  if (type == SubCaptureTarget::Type::kCropTarget) {
    resolver->SetResultSuffix("Result2");
  }
  auto promise = resolver->Promise();

#if BUILDFLAG(IS_ANDROID)
  resolver->Reject<DOMException>(
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                         "Not supported on Android."),
      ApplySubCaptureTargetResult::kUnsupportedPlatform);
  return promise;
#else

  const std::optional<base::Token> token =
      IdStringToToken(target ? target->GetId() : String());
  if (!token.has_value()) {
    resolver->Reject<DOMException>(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                           "Invalid token."),
        ApplySubCaptureTargetResult::kInvalidTarget);
    return promise;
  }

  MediaStreamComponent* const component = Component();
  DCHECK(component);

  MediaStreamSource* const source = component->Source();
  DCHECK(component->Source());
  // We don't currently instantiate BrowserCaptureMediaStreamTrack for audio
  // tracks. If we do in the future, we'll have to raise an exception if
  // cropTo() or restrictTo() are called on a non-video track.
  DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeVideo);

  MediaStreamVideoSource* const native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  MediaStreamTrackPlatform* const native_track =
      MediaStreamTrackPlatform::GetTrack(WebMediaStreamTrack(component));
  if (!native_source || !native_track) {
    resolver->Reject<DOMException>(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError,
                                           "Native/platform track missing."),
        ApplySubCaptureTargetResult::kRejectedWithErrorGeneric);
    return promise;
  }

  // TODO(crbug.com/1332628): Instead of using GetNextSubCaptureTargetVersion(),
  // move the ownership of the Promises from this->pending_promises_ into
  // native_source.
  const std::optional<uint32_t> optional_sub_capture_target_version =
      native_source->GetNextSubCaptureTargetVersion();
  if (!optional_sub_capture_target_version.has_value()) {
    resolver->Reject<DOMException>(
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kOperationError,
            "Can't change target while clones exist."),
        ApplySubCaptureTargetResult::kInvalidTarget);
    return promise;
  }
  const uint32_t sub_capture_target_version =
      optional_sub_capture_target_version.value();

  pending_promises_.Set(sub_capture_target_version,
                        MakeGarbageCollected<PromiseInfo>(resolver));

  // Register for a one-off notification when the first frame cropped
  // to the new crop-target is observed.
  native_track->AddSubCaptureTargetVersionCallback(
      sub_capture_target_version,
      WTF::BindOnce(
          &BrowserCaptureMediaStreamTrack::OnSubCaptureTargetVersionObserved,
          WrapWeakPersistent(this), sub_capture_target_version));

  native_source->ApplySubCaptureTarget(
      type, token.value(), sub_capture_target_version,
      WTF::BindOnce(&BrowserCaptureMediaStreamTrack::OnResultFromBrowserProcess,
                    WrapWeakPersistent(this), sub_capture_target_version));

  return promise;
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void BrowserCaptureMediaStreamTrack::OnResultFromBrowserProcess(
    uint32_t sub_capture_target_version,
    media::mojom::ApplySubCaptureTargetResult result) {
  DCHECK(IsMainThread());
  DCHECK_GT(sub_capture_target_version, 0u);

  const auto iter = pending_promises_.find(sub_capture_target_version);
  if (iter == pending_promises_.end()) {
    return;
  }
  PromiseInfo* const info = iter->value;

  DCHECK(!info->result.has_value()) << "Invoked twice.";
  info->result = result;

  MaybeFinalizeCropPromise(iter);
}

void BrowserCaptureMediaStreamTrack::OnSubCaptureTargetVersionObserved(
    uint32_t sub_capture_target_version) {
  DCHECK(IsMainThread());
  DCHECK_GT(sub_capture_target_version, 0u);

  const auto iter = pending_promises_.find(sub_capture_target_version);
  if (iter == pending_promises_.end()) {
    return;
  }
  PromiseInfo* const info = iter->value;

  DCHECK(!info->sub_capture_target_version_observed) << "Invoked twice.";
  info->sub_capture_target_version_observed = true;

  MaybeFinalizeCropPromise(iter);
}

void BrowserCaptureMediaStreamTrack::MaybeFinalizeCropPromise(
    BrowserCaptureMediaStreamTrack::PromiseMapIterator iter) {
  DCHECK(IsMainThread());
  CHECK_NE(iter, pending_promises_.end(), base::NotFatalUntil::M130);

  PromiseInfo* const info = iter->value;

  if (!info->result.has_value()) {
    return;
  }

  const media::mojom::ApplySubCaptureTargetResult result = info->result.value();

  // Failure can be reported immediately, but success is only reported once
  // the new sub-capture-target-version is observed.
  if (result == media::mojom::ApplySubCaptureTargetResult::kSuccess &&
      !info->sub_capture_target_version_observed) {
    return;
  }

  // When `result == kSuccess`, the callback will be removed by the track
  // itself as it invokes it. For failure, we remove the callback immediately,
  // since there's no need to wait.
  if (result != media::mojom::ApplySubCaptureTargetResult::kSuccess) {
    MediaStreamTrackPlatform* const native_track =
        MediaStreamTrackPlatform::GetTrack(WebMediaStreamTrack(Component()));
    if (native_track) {
      native_track->RemoveSubCaptureTargetVersionCallback(iter->key);
    }
  }

  ScriptPromiseResolverWithTracker<ApplySubCaptureTargetResult,
                                   IDLUndefined>* const resolver =
      info->promise_resolver;
  pending_promises_.erase(iter);
  ResolveApplySubCaptureTargetPromiseHelper(resolver, result);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
