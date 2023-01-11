// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_OBSERVING_INPUT_FILTER_H_
#define REMOTING_PROTOCOL_OBSERVING_INPUT_FILTER_H_

#include <functional>

#include "base/functional/callback.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/input_filter.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace remoting::protocol {

// Filtering InputStub implementation which calls the provided callback when an
// input event is received from the client.
class ObservingInputFilter : public InputFilter {
 public:
  using Event = absl::variant<std::reference_wrapper<const KeyEvent>,
                              std::reference_wrapper<const TextEvent>,
                              std::reference_wrapper<const MouseEvent>,
                              std::reference_wrapper<const TouchEvent>>;

  using InputEventCallback = base::RepeatingCallback<void(Event)>;

  explicit ObservingInputFilter(InputStub* input_stub);

  ObservingInputFilter(const ObservingInputFilter&) = delete;
  ObservingInputFilter& operator=(const ObservingInputFilter&) = delete;

  ~ObservingInputFilter() override;

  // InputStub overrides.
  void InjectKeyEvent(const KeyEvent& event) override;
  void InjectTextEvent(const TextEvent& event) override;
  void InjectMouseEvent(const MouseEvent& event) override;
  void InjectTouchEvent(const TouchEvent& event) override;

  // |on_input_event_| is called for each remote input event received.
  void SetInputEventCallback(InputEventCallback on_input_event);

  // Clears |on_input_event_| which stops notifications from being sent.
  void ClearInputEventCallback();

 private:
  InputEventCallback on_input_event_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_OBSERVING_INPUT_FILTER_H_
