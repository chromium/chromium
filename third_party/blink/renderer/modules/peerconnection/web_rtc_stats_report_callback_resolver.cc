#include "third_party/blink/renderer/modules/peerconnection/web_rtc_stats_report_callback_resolver.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"

namespace blink {

void WebRTCStatsReportCallbackResolver(
    ScriptPromiseResolver<RTCStatsReport>* resolver,
    std::unique_ptr<RTCStatsReportPlatform> report) {
  DCHECK(ExecutionContext::From(resolver->GetScriptState())->IsContextThread());
  resolver->Resolve(MakeGarbageCollected<RTCStatsReport>(std::move(report)));
}

}  // namespace blink
