// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/policy_engine_opcodes.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

namespace {

// Match strings case insensitively.
// Returns `EVAL_TRUE` if `match_str` is a prefix of `source_str` or the
// two strings are equal.
EvalResult MatchStrings(std::wstring_view match_str,
                        std::wstring_view source_str) {
  std::optional<bool> result =
      EqualUnicodeString(match_str, source_str.substr(0, match_str.size()));
  if (!result) {
    return EVAL_ERROR;
  }
  return *result ? EVAL_TRUE : EVAL_FALSE;
}

}  // namespace

// Note: The opcodes are implemented as functions (as opposed to classes derived
// from PolicyOpcode) because you should not add more member variables to the
// PolicyOpcode class since it would cause object slicing on the target. So to
// enforce that (instead of just trusting the developer) the opcodes became
// just functions.
//
// In the code that follows I have keep the evaluation function and the factory
// function together to stress the close relationship between both. For example,
// only the factory method and the evaluation function know the stored argument
// order and meaning.

size_t OpcodeFactory::memory_size() const {
  DCHECK_GE(memory_bottom_, memory_top_);
  return static_cast<size_t>(memory_bottom_ - memory_top_);
}

template <int>
EvalResult OpcodeEval(PolicyOpcode* opcode,
                      const ParameterSet* pp,
                      MatchContext* match);

//////////////////////////////////////////////////////////////////////////////
// Opcode OpAlwaysFalse:
// Does not require input parameter.

PolicyOpcode* OpcodeFactory::MakeOpAlwaysFalse(uint32_t options) {
  return MakeBase(OP_ALWAYS_FALSE, options);
}

template <>
EvalResult OpcodeEval<OP_ALWAYS_FALSE>(PolicyOpcode* opcode,
                                       const ParameterSet* param,
                                       MatchContext* context) {
  return EVAL_FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// Opcode OpAlwaysTrue:
// Does not require input parameter.

PolicyOpcode* OpcodeFactory::MakeOpAlwaysTrue(uint32_t options) {
  return MakeBase(OP_ALWAYS_TRUE, options);
}

template <>
EvalResult OpcodeEval<OP_ALWAYS_TRUE>(PolicyOpcode* opcode,
                                      const ParameterSet* param,
                                      MatchContext* context) {
  return EVAL_TRUE;
}

//////////////////////////////////////////////////////////////////////////////
// Opcode OpAction:
// Does not require input parameter.
// Argument 0 contains the actual action to return.

PolicyOpcode* OpcodeFactory::MakeOpAction(EvalResult action, uint32_t options) {
  PolicyOpcode* opcode = MakeBase(OP_ACTION, options, 0);
  if (!opcode)
    return nullptr;
  opcode->SetArgument(0, action);
  return opcode;
}

template <>
EvalResult OpcodeEval<OP_ACTION>(PolicyOpcode* opcode,
                                 const ParameterSet* param,
                                 MatchContext* context) {
  int action = 0;
  opcode->GetArgument(0, &action);
  return static_cast<EvalResult>(action);
}

//////////////////////////////////////////////////////////////////////////////
// Opcode OpNumberMatch:
// Requires a uint32_t or void* in selected_param
// Argument 0 is the stored number to match.
// Argument 1 is the C++ type of the 0th argument.

PolicyOpcode* OpcodeFactory::MakeOpNumberMatch(uint8_t selected_param,
                                               uint32_t match,
                                               uint32_t options) {
  PolicyOpcode* opcode = MakeBase(OP_NUMBER_MATCH, options, selected_param);
  if (!opcode)
    return nullptr;
  opcode->SetArgument(0, match);
  opcode->SetArgument(1, UINT32_TYPE);
  return opcode;
}

PolicyOpcode* OpcodeFactory::MakeOpVoidPtrMatch(uint8_t selected_param,
                                                const void* match,
                                                uint32_t options) {
  PolicyOpcode* opcode = MakeBase(OP_NUMBER_MATCH, options, selected_param);
  if (!opcode)
    return nullptr;
  opcode->SetArgument(0, match);
  opcode->SetArgument(1, VOIDPTR_TYPE);
  return opcode;
}

template <>
EvalResult OpcodeEval<OP_NUMBER_MATCH>(PolicyOpcode* opcode,
                                       const ParameterSet* param,
                                       MatchContext* context) {
  uint32_t value_uint32 = 0;
  if (param->Get(&value_uint32)) {
    uint32_t match_uint32 = 0;
    opcode->GetArgument(0, &match_uint32);
    return (match_uint32 != value_uint32) ? EVAL_FALSE : EVAL_TRUE;
  } else {
    const void* value_ptr = nullptr;
    if (param->Get(&value_ptr)) {
      const void* match_ptr = nullptr;
      opcode->GetArgument(0, &match_ptr);
      return (match_ptr != value_ptr) ? EVAL_FALSE : EVAL_TRUE;
    }
  }
  return EVAL_ERROR;
}

//////////////////////////////////////////////////////////////////////////////
// Opcode OpNumberAndMatch:
// Requires a uint32_t in selected_param.
// Argument 0 is the stored number to match.

PolicyOpcode* OpcodeFactory::MakeOpNumberAndMatch(uint8_t selected_param,
                                                  uint32_t match,
                                                  uint32_t options) {
  PolicyOpcode* opcode = MakeBase(OP_NUMBER_AND_MATCH, options, selected_param);
  if (!opcode)
    return nullptr;
  opcode->SetArgument(0, match);
  return opcode;
}

template <>
EvalResult OpcodeEval<OP_NUMBER_AND_MATCH>(PolicyOpcode* opcode,
                                           const ParameterSet* param,
                                           MatchContext* context) {
  uint32_t value = 0;
  if (!param->Get(&value))
    return EVAL_ERROR;

  uint32_t number = 0;
  opcode->GetArgument(0, &number);
  return (number & value) ? EVAL_TRUE : EVAL_FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// Opcode OpWStringMatch:
// Requires a wchar_t* in selected_param.
// Argument 0 is the byte displacement of the stored string.
// Argument 1 is the length in chars of the stored string.
// Argument 2 is the offset to apply on the input string. It has special values.
// as noted in the header file.
// Argument 3 is the string matching options (true if last token, 0 otherwise).

PolicyOpcode* OpcodeFactory::MakeOpWStringMatch(uint8_t selected_param,
                                                std::wstring_view match_str,
                                                int start_position,
                                                uint32_t options,
                                                bool last_token) {
  if (match_str.empty()) {
    return nullptr;
  }

  PolicyOpcode* opcode = MakeBase(OP_WSTRING_MATCH, options, selected_param);
  if (!opcode)
    return nullptr;
  ptrdiff_t delta_str = AllocRelative(opcode, match_str);
  if (0 == delta_str)
    return nullptr;
  opcode->SetArgument(0, delta_str);
  opcode->SetArgument(1, match_str.size());
  opcode->SetArgument(2, start_position);
  opcode->SetArgument(3, last_token ? 1 : 0);
  return opcode;
}

template <>
EvalResult OpcodeEval<OP_WSTRING_MATCH>(PolicyOpcode* opcode,
                                        const ParameterSet* param,
                                        MatchContext* context) {
  if (!context) {
    return EVAL_ERROR;
  }

  std::wstring_view source_str;
  if (!param->Get(&source_str)) {
    return EVAL_ERROR;
  }

  // Assume we won't want to match when an empty string is passed.
  if (source_str.empty()) {
    return EVAL_FALSE;
  }

  int start_position = 0;
  size_t match_len = 0;
  unsigned int last_token = 0;
  opcode->GetArgument(1, &match_len);
  opcode->GetArgument(2, &start_position);
  opcode->GetArgument(3, &last_token);

  std::wstring_view match_str(opcode->GetRelativeString(0), match_len);

  // Advance the source string to the last successfully evaluated position
  // according to the match context.
  source_str.remove_prefix(context->position);
  if (source_str.empty()) {
    // If we reached the end of the source string there is nothing we can
    // match against.
    return EVAL_FALSE;
  }

  if (match_str.size() > source_str.size()) {
    // There can't be a positive match when the target string is bigger than
    // the source string
    return EVAL_FALSE;
  }

  // We have three cases, depending on the value of start_pos:
  // Case 1. We skip N characters and compare once.
  // Case 2: We skip to the end and compare once.
  // Case 3: We match the first substring (if we find any).
  EvalResult result = EVAL_FALSE;
  size_t start_offset = 0;
  if (start_position >= 0) {
    start_offset = static_cast<size_t>(start_position);
    if (kSeekToEnd == start_position) {
      start_offset = static_cast<size_t>(source_str.size() - match_str.size());
    } else if (last_token) {
      // A sub-case of case 3 is that the final token needs a full match.
      if ((match_str.size() + start_offset) != source_str.size()) {
        return EVAL_FALSE;
      }
    }

    // Advance start_offset characters. Warning! this does not consider
    // utf16 encodings (surrogate pairs) or other Unicode 'features'.
    source_str.remove_prefix(start_offset);

    // Since we skipped, lets reevaluate just the lengths again.
    if (match_str.size() > source_str.size()) {
      return EVAL_FALSE;
    }

    result = MatchStrings(match_str, source_str);
  } else if (start_position < 0) {
    do {
      result = MatchStrings(match_str, source_str);
      if (result != EVAL_FALSE) {
        break;
      }
      source_str.remove_prefix(1);
      start_offset++;
    } while (source_str.size() >= match_str.size());
  }
  if (result == EVAL_TRUE) {
    // Match! update the match context.
    context->position += start_offset + match_str.size();
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////
// OpcodeMaker (other member functions).

PolicyOpcode* OpcodeFactory::MakeBase(OpcodeID opcode_id, uint32_t options) {
  if (memory_size() < sizeof(PolicyOpcode))
    return nullptr;

  // Create opcode using placement-new on the buffer memory.
  PolicyOpcode* opcode = new (memory_top_) PolicyOpcode();

  // Fill in the standard fields, that every opcode has.
  UNSAFE_TODO(memory_top_ += sizeof(PolicyOpcode));
  opcode->opcode_id_ = opcode_id;
  opcode->SetOptions(options);
  opcode->has_param_ = 0;
  opcode->parameter_ = 0;
  return opcode;
}

PolicyOpcode* OpcodeFactory::MakeBase(OpcodeID opcode_id,
                                      uint32_t options,
                                      uint8_t selected_param) {
  PolicyOpcode* opcode = MakeBase(opcode_id, options);
  if (!opcode)
    return nullptr;
  opcode->has_param_ = 1;
  opcode->parameter_ = selected_param;
  return opcode;
}

ptrdiff_t OpcodeFactory::AllocRelative(void* start, std::wstring_view str) {
  size_t bytes = str.size() * sizeof(wchar_t);
  if (memory_size() < bytes)
    return 0;
  UNSAFE_TODO(memory_bottom_ -= bytes);
  if (reinterpret_cast<UINT_PTR>(memory_bottom_.get()) & 1) {
    // TODO(cpu) replace this for something better.
    ::DebugBreak();
  }
  UNSAFE_TODO(memcpy(memory_bottom_, str.data(), bytes));
  ptrdiff_t delta = memory_bottom_ - reinterpret_cast<char*>(start);
  return delta;
}

//////////////////////////////////////////////////////////////////////////////
// Opcode evaluation dispatchers.

// This function is the one and only entry for evaluating any opcode. It is
// in charge of applying any relevant opcode options and calling EvaluateInner
// were the actual dispatch-by-id is made. It would seem at first glance that
// the dispatch should be done by virtual function (vtable) calls but you have
// to remember that the opcodes are made in the broker process and copied as
// raw memory to the target process.

EvalResult PolicyOpcode::Evaluate(const ParameterSet* call_params,
                                  size_t param_count,
                                  MatchContext* match) {
  if (!call_params)
    return EVAL_ERROR;
  const ParameterSet* selected_param = nullptr;
  if (has_param_) {
    if (parameter_ >= param_count) {
      return EVAL_ERROR;
    }
    selected_param = &UNSAFE_TODO(call_params[parameter_]);
  }
  EvalResult result = EvaluateHelper(selected_param, match);

  // Apply the general options regardless of the particular type of opcode.
  if (kPolNone == options_) {
    return result;
  }

  if (options_ & kPolNegateEval) {
    if (EVAL_TRUE == result) {
      result = EVAL_FALSE;
    } else if (EVAL_FALSE == result) {
      result = EVAL_TRUE;
    } else if (EVAL_ERROR != result) {
      result = EVAL_ERROR;
    }
  }
  if (match) {
    if (options_ & kPolClearContext)
      match->Clear();
    if (options_ & kPolUseOREval)
      match->options = kPolUseOREval;
  }
  return result;
}

#define OPCODE_EVAL(op, x, y, z) \
  case op:                       \
    return OpcodeEval<op>(x, y, z)

EvalResult PolicyOpcode::EvaluateHelper(const ParameterSet* parameters,
                                        MatchContext* match) {
  switch (opcode_id_) {
    OPCODE_EVAL(OP_ALWAYS_FALSE, this, parameters, match);
    OPCODE_EVAL(OP_ALWAYS_TRUE, this, parameters, match);
    OPCODE_EVAL(OP_NUMBER_MATCH, this, parameters, match);
    OPCODE_EVAL(OP_NUMBER_AND_MATCH, this, parameters, match);
    OPCODE_EVAL(OP_WSTRING_MATCH, this, parameters, match);
    OPCODE_EVAL(OP_ACTION, this, parameters, match);
    default:
      return EVAL_ERROR;
  }
}

#undef OPCODE_EVAL

}  // namespace sandbox
