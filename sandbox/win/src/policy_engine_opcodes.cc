// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/policy_engine_opcodes.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "sandbox/win/src/sandbox_nt_types.h"
#include "sandbox/win/src/sandbox_nt_util.h"
#include "sandbox/win/src/sandbox_types.h"

namespace {
const unsigned short kMaxUniStrSize = 0xfffc / sizeof(wchar_t);

bool InitStringUnicode(const wchar_t* source,
                       size_t length,
                       UNICODE_STRING* ustring) {
  if (length > kMaxUniStrSize) {
    return false;
  }
  ustring->Buffer = const_cast<wchar_t*>(source);
  ustring->Length = static_cast<USHORT>(length) * sizeof(wchar_t);
  ustring->MaximumLength = source ? ustring->Length + sizeof(wchar_t) : 0;
  return true;
}

}  // namespace

namespace sandbox {

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
                                                const wchar_t* match_str,
                                                int start_position,
                                                uint32_t options,
                                                bool last_token) {
  if (!match_str)
    return nullptr;
  if ('\0' == match_str[0])
    return nullptr;

  size_t length = wcslen(match_str);

  PolicyOpcode* opcode = MakeBase(OP_WSTRING_MATCH, options, selected_param);
  if (!opcode)
    return nullptr;
  ptrdiff_t delta_str = AllocRelative(opcode, match_str, length + 1);
  if (0 == delta_str)
    return nullptr;
  opcode->SetArgument(0, delta_str);
  opcode->SetArgument(1, length);
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
  const wchar_t* source_str = nullptr;
  if (!param->Get(&source_str))
    return EVAL_ERROR;
  // Assume we won't want to match when a nullptr parameter is passed to the
  // hooked function.
  if (!source_str)
    return EVAL_FALSE;

  int start_position = 0;
  size_t match_len = 0;
  unsigned int last_token = 0;
  opcode->GetArgument(1, &match_len);
  opcode->GetArgument(2, &start_position);
  opcode->GetArgument(3, &last_token);

  const wchar_t* match_str = opcode->GetRelativeString(0);
  // Advance the source string to the last successfully evaluated position
  // according to the match context.
  source_str = &source_str[context->position];
  size_t source_len = GetNtExports()->wcslen(source_str);

  if (0 == source_len) {
    // If we reached the end of the source string there is nothing we can
    // match against.
    return EVAL_FALSE;
  }
  if (match_len > source_len) {
    // There can't be a positive match when the target string is bigger than
    // the source string
    return EVAL_FALSE;
  }

  // We have three cases, depending on the value of start_pos:
  // Case 1. We skip N characters and compare once.
  // Case 2: We skip to the end and compare once.
  // Case 3: We match the first substring (if we find any).
  if (start_position >= 0) {
    size_t start_offset = static_cast<size_t>(start_position);
    if (kSeekToEnd == start_position) {
      start_offset = static_cast<size_t>(source_len - match_len);
    } else if (last_token) {
      // A sub-case of case 3 is that the final token needs a full match.
      if ((match_len + start_offset) != source_len) {
        return EVAL_FALSE;
      }
    }

    // Advance start_pos characters. Warning! this does not consider
    // utf16 encodings (surrogate pairs) or other Unicode 'features'.
    source_str += start_offset;

    // Since we skipped, lets reevaluate just the lengths again.
    if ((match_len + start_offset) > source_len) {
      return EVAL_FALSE;
    }

    UNICODE_STRING match_ustr;
    UNICODE_STRING source_ustr;
    if (!InitStringUnicode(match_str, match_len, &match_ustr) ||
        !InitStringUnicode(source_str, match_len, &source_ustr))
      return EVAL_ERROR;

    if (0 == GetNtExports()->RtlCompareUnicodeString(&match_ustr, &source_ustr,
                                                     TRUE)) {
      // Match! update the match context.
      context->position += start_offset + match_len;
      return EVAL_TRUE;
    } else {
      return EVAL_FALSE;
    }
  } else if (start_position < 0) {
    UNICODE_STRING match_ustr;
    UNICODE_STRING source_ustr;
    if (!InitStringUnicode(match_str, match_len, &match_ustr) ||
        !InitStringUnicode(source_str, match_len, &source_ustr))
      return EVAL_ERROR;

    do {
      if (0 == GetNtExports()->RtlCompareUnicodeString(&match_ustr,
                                                       &source_ustr, TRUE)) {
        // Match! update the match context.
        context->position +=
            static_cast<size_t>(source_ustr.Buffer - source_str) + match_len;
        return EVAL_TRUE;
      }
      ++source_ustr.Buffer;
      --source_len;
    } while (source_len >= match_len);
  }
  return EVAL_FALSE;
}

//////////////////////////////////////////////////////////////////////////////
// OpcodeMaker (other member functions).

PolicyOpcode* OpcodeFactory::MakeBase(OpcodeID opcode_id, uint32_t options) {
  if (memory_size() < sizeof(PolicyOpcode))
    return nullptr;

  // Create opcode using placement-new on the buffer memory.
  PolicyOpcode* opcode = new (memory_top_) PolicyOpcode();

  // Fill in the standard fields, that every opcode has.
  memory_top_ += sizeof(PolicyOpcode);
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

ptrdiff_t OpcodeFactory::AllocRelative(void* start,
                                       const wchar_t* str,
                                       size_t length) {
  size_t bytes = length * sizeof(wchar_t);
  if (memory_size() < bytes)
    return 0;
  memory_bottom_ -= bytes;
  if (reinterpret_cast<UINT_PTR>(memory_bottom_.get()) & 1) {
    // TODO(cpu) replace this for something better.
    ::DebugBreak();
  }
  memcpy(memory_bottom_, str, bytes);
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
    selected_param = &call_params[parameter_];
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
