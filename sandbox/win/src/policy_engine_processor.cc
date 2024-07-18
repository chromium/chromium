// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/policy_engine_processor.h"

#include <stddef.h>
#include <stdint.h>

namespace sandbox {

void PolicyProcessor::SetInternalState(size_t index, EvalResult result) {
  state_.current_index_ = index;
  state_.current_result_ = result;
}

EvalResult PolicyProcessor::GetAction() const {
  return state_.current_result_;
}

// Decides if an opcode can be skipped (not evaluated) or not. The function
// takes as inputs the opcode and the current evaluation context and returns
// true if the opcode should be skipped or not and also can set keep_skipping
// to false to signal that the current instruction should be skipped but not
// the next after the current one.
bool SkipOpcode(const PolicyOpcode& opcode,
                MatchContext* context,
                bool* keep_skipping) {
  if (opcode.IsAction()) {
    uint32_t options = context->options;
    context->Clear();
    *keep_skipping = false;
    return (kPolUseOREval != options);
  }
  *keep_skipping = true;
  return true;
}

PolicyResult PolicyProcessor::Evaluate(uint32_t options,
                                       ParameterSet* parameters,
                                       size_t param_count) {
  if (!policy_)
    return NO_POLICY_MATCH;
  if (0 == policy_->opcode_count)
    return NO_POLICY_MATCH;
  if (!(kShortEval & options))
    return POLICY_ERROR;

  MatchContext context;
  bool evaluation = false;
  bool skip_group = false;
  SetInternalState(0, EVAL_FALSE);
  size_t count = policy_->opcode_count;

  // Loop over all the opcodes Evaluating in sequence. Since we only support
  // short circuit evaluation, we stop as soon as we find an 'action' opcode
  // and the current evaluation is true.
  //
  // Skipping opcodes can happen when we are in AND mode (!kPolUseOREval) and
  // have got EVAL_FALSE or when we are in OR mode (kPolUseOREval) and got
  // EVAL_TRUE. Skipping will stop at the next action opcode or at the opcode
  // after the action depending on kPolUseOREval.

  for (size_t ix = 0; ix != count; ++ix) {
    PolicyOpcode& opcode = policy_->opcodes[ix];
    // Skipping block.
    if (skip_group) {
      if (SkipOpcode(opcode, &context, &skip_group))
        continue;
    }
    // Evaluation block.
    EvalResult result = opcode.Evaluate(parameters, param_count, &context);
    switch (result) {
      case EVAL_FALSE:
        evaluation = false;
        if (kPolUseOREval != context.options)
          skip_group = true;
        break;
      case EVAL_ERROR:
        if (kStopOnErrors & options)
          return POLICY_ERROR;
        break;
      case EVAL_TRUE:
        evaluation = true;
        if (kPolUseOREval == context.options)
          skip_group = true;
        break;
      default:
        // We have evaluated an action.
        SetInternalState(ix, result);
        return POLICY_MATCH;
    }
  }

  if (evaluation) {
    // Reaching the end of the policy with a positive evaluation is probably
    // an error: we did not find a final action opcode?
    return POLICY_ERROR;
  }
  return NO_POLICY_MATCH;
}

}  // namespace sandbox
