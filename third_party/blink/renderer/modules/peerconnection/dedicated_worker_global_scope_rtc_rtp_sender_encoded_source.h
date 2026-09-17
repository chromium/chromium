#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_DEDICATED_WORKER_GLOBAL_SCOPE_RTC_RTP_SENDER_ENCODED_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_DEDICATED_WORKER_GLOBAL_SCOPE_RTC_RTP_SENDER_ENCODED_SOURCE_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DedicatedWorkerGlobalScopeRTCRtpSenderEncodedSource {
  STATIC_ONLY(DedicatedWorkerGlobalScopeRTCRtpSenderEncodedSource);

 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(rtcsenderencodedsource,
                                         kRtcsenderencodedsource)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_DEDICATED_WORKER_GLOBAL_SCOPE_RTC_RTP_SENDER_ENCODED_SOURCE_H_
