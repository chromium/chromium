// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_FILTER_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_FILTER_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AudioNode;

class IIRFilterHandler final : public AudioBasicProcessorHandler {
 public:
  static scoped_refptr<IIRFilterHandler> Create(
      AudioNode&,
      float sample_rate,
      const Vector<double>& feedforward_coef,
      const Vector<double>& feedback_coef,
      bool is_filter_stable);

  void Process(uint32_t frames_to_process) override;

 private:
  IIRFilterHandler(AudioNode&,
                   float sample_rate,
                   const Vector<double>& feedforward_coef,
                   const Vector<double>& feedback_coef,
                   bool is_filter_stable);

  void NotifyBadState() const;

  // Only notify the user of the once.  No need to spam the console with
  // messages, because once we're in a bad state, it usually stays that way
  // forever.  Only accessed from audio thread.
  bool did_warn_bad_filter_state_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<IIRFilterHandler> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_IIR_FILTER_HANDLER_H_
