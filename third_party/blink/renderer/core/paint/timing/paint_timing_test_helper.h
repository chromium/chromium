#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_HELPER_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callback_manager.h"

namespace blink {

// |MockPaintTimingCallbackManager| is used to mock
// |ChromeClient::NotifyPresentationTime()|'s presentation-time queueing and
// invoking for unit-tests. Find more details in |PaintTimingCallbackManager|.
class MockPaintTimingCallbackManager final
    : public GarbageCollected<MockPaintTimingCallbackManager>,
      public PaintTimingCallbackManager {
 public:
  ~MockPaintTimingCallbackManager() {}
  void RegisterCallback(
      PaintTimingCallbackManager::LocalThreadCallback callback) override {
    callback_queue_.push(std::move(callback));
  }
  void InvokePresentationTimeCallback(base::TimeTicks presentation_time) {
    DCHECK_GT(callback_queue_.size(), 0UL);
    std::move(callback_queue_.front()).Run(presentation_time);
    callback_queue_.pop();
  }

  size_t CountCallbacks() { return callback_queue_.size(); }

  void Trace(Visitor* visitor) const override {}

 private:
  PaintTimingCallbackManager::CallbackQueue callback_queue_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_PAINT_TIMING_TEST_HELPER_H_
