// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/policy_low_level.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/strings/string_split_win.h"

namespace sandbox {

LowLevelPolicy::LowLevelPolicy(PolicyGlobal* policy_store)
    : policy_store_(policy_store) {}

// Adding a rule is nothing more than pushing it into an stl container. Done()
// is called for the rule in case the code that made the rule in the first
// place has not done it.
bool LowLevelPolicy::AddRule(IpcTag service, PolicyRule rule) {
  if (!rule.Done()) {
    return false;
  }

  rules_[service].emplace_back(std::move(rule));
  return true;
}

LowLevelPolicy::~LowLevelPolicy() = default;

// Here is where the heavy byte shuffling is done. We take all the rules and
// 'compile' them into a single memory region. We enter the big for-loop where
// we carve a memory zone for the opcodes and the data and call RebindCopy on
// each rule so they all end up nicely packed in the policy_store_.
bool LowLevelPolicy::Done() {
  PolicyBuffer* current_buffer = &policy_store_->data[0];
  char* buffer_end = UNSAFE_TODO(reinterpret_cast<char*>(current_buffer) +
                                 policy_store_->data_size);
  size_t avail_size = policy_store_->data_size;

  for (const auto& rule_entry : rules_) {
    IpcTag service = rule_entry.first;
    if (service > IpcTag::kMaxValue) {
      return false;
    }
    policy_store_->SetService(service, current_buffer);
    size_t svc_opcode_count = 0;

    for (const auto& rule : rule_entry.second) {
      size_t op_count = rule.GetOpcodeCount();

      size_t opcodes_size = op_count * sizeof(PolicyOpcode);
      if (avail_size < opcodes_size) {
        return false;
      }
      size_t data_size = avail_size - opcodes_size;
      PolicyOpcode* opcodes_start =
          &UNSAFE_TODO(current_buffer->opcodes[svc_opcode_count]);
      if (!rule.RebindCopy(opcodes_start, opcodes_size, buffer_end,
                           &data_size)) {
        return false;
      }
      size_t used = avail_size - data_size;
      UNSAFE_TODO(buffer_end -= used);
      avail_size -= used;
      svc_opcode_count += op_count;
    }

    current_buffer->opcode_count = svc_opcode_count;
    size_t policy_buffers_occupied =
        (svc_opcode_count * sizeof(PolicyOpcode)) / sizeof(current_buffer[0]);
    current_buffer = &UNSAFE_TODO(current_buffer[policy_buffers_occupied + 1]);
  }

  return true;
}

PolicyRule::PolicyRule(EvalResult action, uintptr_t constant)
    : action_(action), constant_(constant), done_(false) {
  const size_t kRuleBufferSize = 1024 * 4;
  opcode_buffer_ = base::HeapArray<uint8_t>::WithSize(sizeof(PolicyBuffer) +
                                                      kRuleBufferSize);
  std::ranges::fill(opcode_buffer_, 0);
  opcode_factory_ =
      std::make_unique<OpcodeFactory>(get_buffer(), kRuleBufferSize);
}

PolicyRule::PolicyRule(PolicyRule&& other) noexcept = default;
PolicyRule& PolicyRule::operator=(PolicyRule&& other) noexcept = default;

// This function get called from a simple state machine implemented in
// AddStringMatch() which passes the pending asterisk state and it passes
// true in last_call if AddStringMatch() has finished processing the input
// pattern string and this would be the last call to generate any pending
// opcode.
bool PolicyRule::GenStringOpcode(RuleType rule_type,
                                 uint8_t parameter,
                                 bool pending_asterisk,
                                 bool last_call,
                                 std::wstring_view fragment,
                                 PolicyOpcode** last_op) {
  // The last opcode must:
  //   1) Always clear the context.
  //   2) Preserve the negation.
  //   3) Remove the 'OR' mode flag.
  uint32_t options = kPolNone;
  if (last_call) {
    if (IF_NOT == rule_type) {
      options = kPolClearContext | kPolNegateEval;
    } else {
      options = kPolClearContext;
    }
  } else if (IF_NOT == rule_type) {
    options = kPolUseOREval | kPolNegateEval;
  }

  // The fragment string contains the accumulated characters to match with, it
  // never contains wildcards (unless they have been escaped) and while there
  // is no fragment there is no new string match opcode to generate.
  if (fragment.empty()) {
    // There is no new opcode to generate but in the last call we have to fix
    // the previous opcode because it was really the last but we did not know
    // it at that time.
    if (last_call && *last_op) {
      (*last_op)->SetOptions(options);
    }
    return true;
  }

  if (pending_asterisk) {
    *last_op = opcode_factory_->MakeOpWStringMatch(
        parameter, fragment, last_call ? kSeekToEnd : kSeekForward, options,
        false);

  } else {
    *last_op = opcode_factory_->MakeOpWStringMatch(parameter, fragment, 0,
                                                   options, last_call);
  }

  if (!*last_op) {
    return false;
  }
  ++get_buffer()->opcode_count;
  return true;
}

bool PolicyRule::AddStringMatch(RuleType rule_type,
                                uint8_t parameter,
                                std::wstring_view string) {
  if (done_) {
    // Do not allow to add more rules after generating the action opcode.
    return false;
  }

  // An empty string or an instance of '**' is an error.
  if (string.empty() || string.contains(L"**")) {
    return false;
  }

  auto fragments = base::SplitStringPiece(string, L"*", base::KEEP_WHITESPACE,
                                          base::SPLIT_WANT_ALL);
  bool pending_asterisk = false;
  PolicyOpcode* last_op = nullptr;
  for (size_t index = 0; index < fragments.size() - 1; index++) {
    if (!GenStringOpcode(rule_type, parameter, pending_asterisk, false,
                         fragments[index], &last_op)) {
      return false;
    }
    pending_asterisk = true;
  }
  return GenStringOpcode(rule_type, parameter, pending_asterisk, true,
                         fragments.back(), &last_op);
}

bool PolicyRule::AddNumberMatch(RuleType rule_type,
                                uint8_t parameter,
                                uint32_t number,
                                RuleOp comparison_op) {
  if (done_) {
    // Do not allow to add more rules after generating the action opcode.
    return false;
  }
  uint32_t opts = (rule_type == IF_NOT) ? kPolNegateEval : kPolNone;

  if (EQUAL == comparison_op) {
    if (!opcode_factory_->MakeOpNumberMatch(parameter, number, opts))
      return false;
  } else if (AND == comparison_op) {
    if (!opcode_factory_->MakeOpNumberAndMatch(parameter, number, opts))
      return false;
  }
  ++get_buffer()->opcode_count;
  return true;
}

bool PolicyRule::Done() {
  if (done_) {
    return true;
  }
  if (!opcode_factory_->MakeOpAction(action_, constant_)) {
    return false;
  }
  ++get_buffer()->opcode_count;
  done_ = true;
  return true;
}

bool PolicyRule::RebindCopy(PolicyOpcode* opcode_start,
                            size_t opcode_size,
                            char* data_start,
                            size_t* data_size) const {
  const PolicyBuffer* buffer = get_buffer();
  size_t count = buffer->opcode_count;
  for (size_t ix = 0; ix != count; ++ix) {
    if (opcode_size < sizeof(PolicyOpcode)) {
      return false;
    }
    const PolicyOpcode& opcode = UNSAFE_TODO(buffer->opcodes[ix]);
    *opcode_start = opcode;
    if (OP_WSTRING_MATCH == opcode.GetID()) {
      // For this opcode argument 0 is a delta to the string and argument 1
      // is the length (in chars) of the string.
      const wchar_t* str = opcode.GetRelativeString(0);
      size_t str_len;
      opcode.GetArgument(1, &str_len);
      str_len = str_len * sizeof(wchar_t);
      if ((*data_size) < str_len) {
        return false;
      }
      *data_size -= str_len;
      UNSAFE_TODO(data_start -= str_len);
      UNSAFE_TODO(memcpy(data_start, str, str_len));
      // Recompute the string displacement
      ptrdiff_t delta = data_start - reinterpret_cast<char*>(opcode_start);
      opcode_start->SetArgument(0, delta);
    }
    UNSAFE_TODO(++opcode_start);
    opcode_size -= sizeof(PolicyOpcode);
  }

  return true;
}

PolicyRule::~PolicyRule() = default;

}  // namespace sandbox
