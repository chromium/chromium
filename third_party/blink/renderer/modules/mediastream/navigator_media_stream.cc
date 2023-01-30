/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "third_party/blink/renderer/modules/mediastream/navigator_media_stream.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_navigator_user_media_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_navigator_user_media_success_callback.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
class V8Callbacks final : public blink::UserMediaRequest::Callbacks {
 public:
  V8Callbacks(V8NavigatorUserMediaSuccessCallback* success_callback,
              V8NavigatorUserMediaErrorCallback* error_callback)
      : success_callback_(success_callback), error_callback_(error_callback) {}
  ~V8Callbacks() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(success_callback_);
    visitor->Trace(error_callback_);
    UserMediaRequest::Callbacks::Trace(visitor);
  }

  void OnSuccess(const MediaStreamVector& streams,
                 CaptureController* capture_controller) override {
    DCHECK_EQ(streams.size(), 1u);
    success_callback_->InvokeAndReportException(nullptr, streams[0]);
  }

  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error,
               CaptureController* capture_controller,
               UserMediaRequestResult result) override {
    error_callback_->InvokeAndReportException(callback_this_value, error);
  }

 private:
  Member<V8NavigatorUserMediaSuccessCallback> success_callback_;
  Member<V8NavigatorUserMediaErrorCallback> error_callback_;
};
}  // namespace

void NavigatorMediaStream::getUserMedia(
    Navigator& navigator,
    const MediaStreamConstraints* options,
    V8NavigatorUserMediaSuccessCallback* success_callback,
    V8NavigatorUserMediaErrorCallback* error_callback,
    ExceptionState& exception_state) {
  DCHECK(success_callback);
  DCHECK(error_callback);

  if (!navigator.DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "No user media client available; is this a detached window?");
    return;
  }

  UserMediaClient* user_media = UserMediaClient::From(navigator.DomWindow());
  // Navigator::DomWindow() should not return a non-null detached window, so we
  // should also successfully get a UserMediaClient from it.
  DCHECK(user_media) << "Missing UserMediaClient on a non-null DomWindow";

  IdentifiableSurface surface;
  constexpr IdentifiableSurface::Type surface_type =
      IdentifiableSurface::Type::kNavigator_GetUserMedia;
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(surface_type)) {
    surface = IdentifiableSurface::FromTypeAndToken(
        surface_type, TokenFromConstraints(options));
  }

  UserMediaRequest* request = UserMediaRequest::Create(
      navigator.DomWindow(), user_media, UserMediaRequestType::kUserMedia,
      options,
      MakeGarbageCollected<V8Callbacks>(success_callback, error_callback),
      exception_state, surface);
  if (!request) {
    DCHECK(exception_state.HadException());
    RecordIdentifiabilityMetric(
        surface, navigator.GetExecutionContext(),
        IdentifiabilityBenignStringToken(exception_state.Message()));
    return;
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    request->Fail(
        mojom::blink::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN,
        error_message);
    RecordIdentifiabilityMetric(
        surface, navigator.GetExecutionContext(),
        IdentifiabilityBenignStringToken(error_message));
    return;
  }

  request->Start();
}

}  // namespace blink
