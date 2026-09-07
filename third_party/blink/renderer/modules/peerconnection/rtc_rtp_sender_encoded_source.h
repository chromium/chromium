#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_ENCODED_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_ENCODED_SOURCE_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/webrtc/api/encoded_video_frame_injector_interface.h"

namespace blink {

class WritableStream;
class RTCRtpSender;

class MODULES_EXPORT RTCRtpSenderEncodedSource : public EventTarget {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Event* CreateVideoEncodedSource(
      CrossThreadWeakHandle<RTCRtpSender> weak_sender,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      CrossThreadHandle<ScriptPromiseResolver<IDLUndefined>> resolver_handle,
      ScriptState* worker_script_state,
      CustomEventMessage data);

  RTCRtpSenderEncodedSource(ScriptState* script_state, const String& kind);
  ~RTCRtpSenderEncodedSource() override = default;

  // EventTarget implementation
  const AtomicString& InterfaceName() const override {
    static const AtomicString& name = AtomicString("RTCRtpSenderEncodedSource");
    return name;
  }
  ExecutionContext* GetExecutionContext() const override {
    return execution_context_.Get();
  }

  // EventHandler attributes
  DEFINE_ATTRIBUTE_EVENT_LISTENER(keyframerequest, kKeyframerequest)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(bitrateinfochange, kBitrateinfochange)

  WritableStream* writable() const { return writable_.Get(); }

  int32_t allocatedBitrate() const { return allocated_bitrate_; }
  int32_t availableOutgoingBitrate() const {
    return available_outgoing_bitrate_;
  }

  void InitializeVideoSink(
      scoped_refptr<webrtc::EncodedVideoFrameInjectorInterface> injector);

  void HandleBitrateInfoChange(int32_t allocated_bitrate,
                               int32_t available_outgoing_bitrate);
  void HandleKeyFrameRequest();

  void Trace(Visitor*) const override;

 private:
  Member<ExecutionContext> execution_context_;
  Member<WritableStream> writable_;

  int32_t allocated_bitrate_ = 0;
  int32_t available_outgoing_bitrate_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SENDER_ENCODED_SOURCE_H_
