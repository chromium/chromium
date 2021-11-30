// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

#include "base/guid.h"
#include "base/token.h"
#include "build/build_config.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/capture/video_capture_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

#if !defined(OS_ANDROID)
// If crop_id is the empty string, returns an empty base::Token.
// If crop_id is a valid UUID, returns a base::Token representing the ID.
// Otherwise, returns nullopt.
absl::optional<base::Token> CropIdStringToToken(const String& crop_id) {
  if (crop_id.IsEmpty()) {
    return base::Token();
  }
  if (!crop_id.ContainsOnlyASCIIOrEmpty()) {
    return absl::nullopt;
  }
  const base::GUID guid = base::GUID::ParseCaseInsensitive(crop_id.Ascii());
  return guid.is_valid() ? absl::make_optional<base::Token>(GUIDToToken(guid))
                         : absl::nullopt;
}

void RaiseCropException(ScriptPromiseResolver* resolver,
                        DOMExceptionCode exception_code,
                        const WTF::String& exception_text) {
  resolver->Reject(
      MakeGarbageCollected<DOMException>(exception_code, exception_text));
}

void ResolveCropPromise(ScriptPromiseResolver* resolver,
                        media::mojom::CropRequestResult result) {
  DCHECK(IsMainThread());

  if (!resolver) {
    return;
  }

  switch (result) {
    case media::mojom::CropRequestResult::kSuccess:
      // TODO(crbug.com/1247761): Delay reporting success to the Web-application
      // until "seeing" the last frame cropped to the previous crop-target.
      resolver->Resolve();
      return;
    case media::mojom::CropRequestResult::kErrorGeneric:
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Unknown error.");
      return;
    case media::mojom::CropRequestResult::kUnsupportedCaptureDevice:
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Unsupported device.");
      return;
    case media::mojom::CropRequestResult::kErrorUnknownDeviceId:
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Unknown device.");
      return;
    case media::mojom::CropRequestResult::kNotImplemented:
      RaiseCropException(resolver, DOMExceptionCode::kAbortError,
                         "Not implemented.");
      return;
  }

  NOTREACHED();
}
#endif

}  // namespace

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : BrowserCaptureMediaStreamTrack(execution_context,
                                     component,
                                     component->Source()->GetReadyState(),
                                     std::move(callback),
                                     descriptor_id,
                                     is_clone) {}

BrowserCaptureMediaStreamTrack::BrowserCaptureMediaStreamTrack(
    ExecutionContext* execution_context,
    MediaStreamComponent* component,
    MediaStreamSource::ReadyState ready_state,
    base::OnceClosure callback,
    const String& descriptor_id,
    bool is_clone)
    : FocusableMediaStreamTrack(execution_context,
                                component,
                                ready_state,
                                std::move(callback),
                                descriptor_id,
                                is_clone) {}

ScriptPromise BrowserCaptureMediaStreamTrack::cropTo(
    ScriptState* script_state,
    const String& crop_id,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

#if defined(OS_ANDROID)
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kUnknownError, "Not supported on Android."));
  return promise;
#else

  const absl::optional<base::Token> crop_id_token =
      CropIdStringToToken(crop_id);
  if (!crop_id_token.has_value()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "Invalid crop-ID."));
    return promise;
  }

  // We don't currently instantiate BrowserCaptureMediaStreamTrack for audio
  // tracks. If we do in the future, we'll have to raise an exception if
  // cropTo() is called on a non-video track.
  DCHECK(Component());
  DCHECK(Component()->Source());
  MediaStreamSource* const source = Component()->Source();
  DCHECK_EQ(source->GetType(), MediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* const native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);

  native_source->Crop(crop_id_token.value(),
                      WTF::Bind(&ResolveCropPromise, WrapPersistent(resolver)));

  return promise;
#endif
}

BrowserCaptureMediaStreamTrack* BrowserCaptureMediaStreamTrack::clone(
    ScriptState* script_state) {
  // Instantiate the clone.
  BrowserCaptureMediaStreamTrack* cloned_track =
      MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
          ExecutionContext::From(script_state), Component()->Clone(),
          GetReadyState(), base::DoNothing(), descriptor_id(),
          /*is_clone=*/true);

  // Copy state.
  CloneInternal(cloned_track);

  return cloned_track;
}

void BrowserCaptureMediaStreamTrack::CloneInternal(
    BrowserCaptureMediaStreamTrack* cloned_track) {
  // Clone parent classes' state.
  FocusableMediaStreamTrack::CloneInternal(cloned_track);

  // No own state to clone. The pending Promises related to cropping are all
  // associated with the original track, and do no need to be cloned.
  // The track itself starts out uncropped.
}

}  // namespace blink
