// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_POLICY_ENGINE_PROCESSOR_H__
#define SANDBOX_SRC_POLICY_ENGINE_PROCESSOR_H__

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_engine_params.h"

namespace sandbox {

// This header contains the core policy evaluator. In its simplest form
// it evaluates a stream of opcodes assuming that they are laid out in
// memory as opcode groups.
//
// An opcode group has N comparison opcodes plus 1 action opcode. For
// example here we have 3 opcode groups (A, B,C):
//
// [comparison 1]  <-- group A start
// [comparison 2]
// [comparison 3]
// [action A    ]
// [comparison 1]  <-- group B start
// [action B    ]
// [comparison 1]  <-- group C start
// [comparison 2]
// [action C    ]
//
// The opcode evaluator proceeds from the top, evaluating each opcode in
// sequence. An opcode group is evaluated until the first comparison that
// returns false. At that point the rest of the group is skipped and evaluation
// resumes with the first comparison of the next group. When all the comparisons
// in a group have evaluated to true and the action is reached. The group is
// considered a matching group.
//
// In the 'ShortEval' mode evaluation stops when it reaches the end or the first
// matching group. The action opcode from this group is the resulting policy
// action.
//
// In the 'RankedEval' mode evaluation stops only when it reaches the end of the
// the opcode stream. In the process all matching groups are saved and at the
// end the 'best' group is selected (what makes the best is TBD) and the action
// from this group is the resulting policy action.
//
// As explained above, the policy evaluation of a group is a logical AND of
// the evaluation of each opcode. However an opcode can request kPolUseOREval
// which makes the evaluation to use logical OR. Given that each opcode can
// request its evaluation result to be negated with kPolNegateEval you can
// achieve the negation of the total group evaluation. This means that if you
// need to express:
//             if (!(c1 && c2 && c3))
// You can do it by:
//             if ((!c1) || (!c2) || (!c3))
//

// Possible outcomes of policy evaluation.
enum PolicyResult { NO_POLICY_MATCH, POLICY_MATCH, POLICY_ERROR };

// Policy evaluation flags
// TODO(cpu): implement the options kStopOnErrors & kRankedEval.
//
// Stop evaluating as soon as an error is encountered.
const uint32_t kStopOnErrors = 1;
// Ignore all non fatal opcode evaluation errors.
const uint32_t kIgnoreErrors = 2;
// Short-circuit evaluation: Only evaluate until opcode group that
// evaluated to true has been found.
const uint32_t kShortEval = 4;
// Discussed briefly at the policy design meeting. It will evaluate
// all rules and then return the 'best' rule that evaluated true.
const uint32_t kRankedEval = 8;

// This class evaluates a policy-opcode stream given the memory where the
// opcodes are and an input 'parameter set'.
//
// This class is designed to be callable from interception points
// as low as the NtXXXX service level (it is not currently safe, but
// it is designed to be made safe).
//
// Its usage in an interception is:
//
//   POLPARAMS_BEGIN(eval_params)
//     POLPARAM(param1)
//     POLPARAM(param2)
//     POLPARAM(param3)
//     POLPARAM(param4)
//     POLPARAM(param5)
//   POLPARAMS_END;
//
//   PolicyProcessor pol_evaluator(policy_memory);
//   PolicyResult pr = pol_evaluator.Evaluate(ShortEval, eval_params,
//                                            _countof(eval_params));
//   if (NO_POLICY_MATCH == pr) {
//     EvalResult policy_action =  pol_evaluator.GetAction();
//     // apply policy here...
//   }
//
// Where the POLPARAM() arguments are derived from the intercepted function
// arguments, and represent all the 'interesting' policy inputs, and
// policy_memory is a memory buffer containing the opcode stream that is the
// relevant policy for this intercept.
class PolicyProcessor {
 public:
  // policy_buffer contains opcodes made with OpcodeFactory. They are usually
  // created in the broker process and evaluated in the target process.

  // This constructor is just a variant of the previous constructor.
  explicit PolicyProcessor(PolicyBuffer* policy) : policy_(policy) {
    SetInternalState(0, EVAL_FALSE);
  }

  // Evaluates a policy-opcode stream. See the comments at the top of this
  // class for more info. Returns POLICY_MATCH if a rule set was found that
  // matches an active policy.
  PolicyResult Evaluate(uint32_t options,
                        ParameterSet* parameters,
                        size_t parameter_count);

  // If the result of Evaluate() was POLICY_MATCH, calling this function returns
  // the recommended policy action.
  EvalResult GetAction() const;

 private:
  struct {
    size_t current_index_;
    EvalResult current_result_;
  } state_;

  // Sets the currently matching action result.
  void SetInternalState(size_t index, EvalResult result);

  PolicyBuffer* policy_;
  DISALLOW_COPY_AND_ASSIGN(PolicyProcessor);
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_POLICY_ENGINE_PROCESSOR_H__
