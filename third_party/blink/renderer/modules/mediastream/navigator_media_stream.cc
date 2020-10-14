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
#include "third_party/blink/renderer/modules/mediastream/media_error_state.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

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
        "No user media controller available; is this a detached window?");
    return;
  }

  UserMediaController* user_media =
      UserMediaController::From(navigator.DomWindow());
  IdentifiableSurface surface;
  constexpr IdentifiableSurface::Type surface_type =
      IdentifiableSurface::Type::kNavigator_GetUserMedia;
  if (IdentifiabilityStudySettings::Get()->IsTypeAllowed(surface_type)) {
    surface = IdentifiableSurface::FromTypeAndToken(
        surface_type, TokenFromConstraints(options));
  }
  MediaErrorState error_state;
  UserMediaRequest* request = UserMediaRequest::Create(
      navigator.DomWindow(), user_media, options, success_callback,
      error_callback, error_state, surface);
  if (!request) {
    DCHECK(error_state.HadException());
    if (error_state.CanGenerateException()) {
      error_state.RaiseException(exception_state);
    } else {
      error_callback->InvokeAndReportException(nullptr,
                                               error_state.CreateError());
    }
    RecordIdentifiabilityMetric(
        surface, navigator.GetExecutionContext(),
        IdentifiabilityBenignStringToken(error_state.GetErrorMessage()));
    return;
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    request->Fail(UserMediaRequest::Error::kSecurityError, error_message);
    RecordIdentifiabilityMetric(
        surface, navigator.GetExecutionContext(),
        IdentifiabilityBenignStringToken(error_message));
    return;
  }

  request->Start();
}

}  // namespace blink
