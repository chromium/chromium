#include "third_party/blink/renderer/modules/mediastream/screen_capture_media_stream_track.h"

#include "base/functional/callback_helpers.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_settings.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/screen_details/screen_detailed.h"
#include "third_party/blink/renderer/modules/screen_details/screen_details.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScreenCaptureMediaStreamTrack::ScreenCaptureMediaStreamTrack(
    ExecutionContext* context,
    MediaStreamComponent* component,
    ScreenDetails* screen_details,
    ScreenDetailed* screen_detailed)
    : MediaStreamTrackImpl(context,
                           component,
                           component->Source()->GetReadyState(),
                           /*callback=*/base::DoNothing()),
      screen_details_(screen_details),
      screen_detailed_(screen_detailed) {}

ScreenDetailed* ScreenCaptureMediaStreamTrack::screenDetailed(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DCHECK(script_state);

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return nullptr;
  }

  if (!screen_detailed_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The ScreenDetailed object could not be created.");
  }
  return screen_detailed_.Get();
}

void ScreenCaptureMediaStreamTrack::Trace(Visitor* visitor) const {
  visitor->Trace(screen_details_);
  visitor->Trace(screen_detailed_);
  MediaStreamTrackImpl::Trace(visitor);
}

}  // namespace blink
