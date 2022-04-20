#include "third_party/blink/renderer/core/frame/pending_beacon.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_beacon_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_beacon_state.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

// static
PendingBeacon* PendingBeacon::Create(ExecutionContext* ec,
                                     const String& targetURL) {
  BeaconOptions* options = BeaconOptions::Create();
  return PendingBeacon::Create(ec, targetURL, options);
}
// static
PendingBeacon* PendingBeacon::Create(ExecutionContext* ec,
                                     const String& targetURL,
                                     BeaconOptions* options) {
  PendingBeacon* beacon = MakeGarbageCollected<PendingBeacon>();
  beacon->url_ = targetURL;
  beacon->state_ = V8BeaconState(V8BeaconState::Enum::kPending);
  beacon->method_ = options->method();
  if (options->hasPageHideTimeout()) {
    beacon->pageHideTimeout_ = options->pageHideTimeout();
  }
  // TODO: Establish/re-use mojo channel to PendingBeaconHost.
  return beacon;
}

void PendingBeacon::setUrl(const String& url) {
  url_ = url;
}

void PendingBeacon::setMethod(const String& method) {
  method_ = method;
}

void PendingBeacon::setPageHideTimeout(int32_t pageHideTimeout) {
  pageHideTimeout_ = pageHideTimeout;
}

void PendingBeacon::setData(
    const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data) {
  // TODO: Implement passing data to the PendingBeaconHost.
  NOTIMPLEMENTED();
}

void PendingBeacon::deactivate() {
  // TODO: Implement beacon deactivation
  NOTIMPLEMENTED();
}

}  // namespace blink
