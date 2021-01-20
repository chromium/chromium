// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/policy_low_level.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

namespace {

// A single rule can use at most this amount of memory.
const size_t kRuleBufferSize = 1024 * 4;

// The possible states of the string matching opcode generator.
enum {
  PENDING_NONE,
  PENDING_ASTERISK,  // Have seen an '*' but have not generated an opcode.
  PENDING_QMARK,     // Have seen an '?' but have not generated an opcode.
};

// The category of the last character seen by the string matching opcode
// generator.
const uint32_t kLastCharIsNone = 0;
const uint32_t kLastCharIsAlpha = 1;
const uint32_t kLastCharIsWild = 2;
const uint32_t kLastCharIsAsterisk = kLastCharIsWild + 4;
const uint32_t kLastCharIsQuestionM = kLastCharIsWild + 8;

}  // namespace

namespace sandbox {

LowLevelPolicy::LowLevelPolicy(PolicyGlobal* policy_store)
    : policy_store_(policy_store) {}

// Adding a rule is nothing more than pushing it into an stl container. Done()
// is called for the rule in case the code that made the rule in the first
// place has not done it.
bool LowLevelPolicy::AddRule(IpcTag service, PolicyRule* rule) {
  if (!rule->Done()) {
    return false;
  }

  PolicyRule* local_rule = new PolicyRule(*rule);
  RuleNode node = {local_rule, service};
  rules_.push_back(node);
  return true;
}

LowLevelPolicy::~LowLevelPolicy() {
  // Delete all the rules.
  typedef std::list<RuleNode> RuleNodes;
  for (RuleNodes::iterator it = rules_.begin(); it != rules_.end(); ++it) {
    delete it->rule;
  }
}

// Here is where the heavy byte shuffling is done. We take all the rules and
// 'compile' them into a single memory region. Now, the rules are in random
// order so the first step is to reorganize them into a stl map that is keyed
// by the service id and as a value contains a list with all the rules that
// belong to that service. Then we enter the big for-loop where we carve a
// memory zone for the opcodes and the data and call RebindCopy on each rule
// so they all end up nicely packed in the policy_store_.
bool LowLevelPolicy::Done() {
  typedef std::list<RuleNode> RuleNodes;
  typedef std::list<const PolicyRule*> RuleList;
  typedef std::map<IpcTag, RuleList> Mmap;
  Mmap mmap;

  for (RuleNodes::iterator it = rules_.begin(); it != rules_.end(); ++it) {
    mmap[it->service].push_back(it->rule);
  }

  PolicyBuffer* current_buffer = &policy_store_->data[0];
  char* buffer_end =
      reinterpret_cast<char*>(current_buffer) + policy_store_->data_size;
  size_t avail_size = policy_store_->data_size;

  for (Mmap::iterator it = mmap.begin(); it != mmap.end(); ++it) {
    IpcTag service = (*it).first;
    if (static_cast<size_t>(service) >= kMaxServiceCount) {
      return false;
    }
    policy_store_->entry[static_cast<size_t>(service)] = current_buffer;

    RuleList::iterator rules_it = (*it).second.begin();
    RuleList::iterator rules_it_end = (*it).second.end();

    size_t svc_opcode_count = 0;

    for (; rules_it != rules_it_end; ++rules_it) {
      const PolicyRule* rule = (*rules_it);
      size_t op_count = rule->GetOpcodeCount();

      size_t opcodes_size = op_count * sizeof(PolicyOpcode);
      if (avail_size < opcodes_size) {
        return false;
      }
      size_t data_size = avail_size - opcodes_size;
      PolicyOpcode* opcodes_start = &current_buffer->opcodes[svc_opcode_count];
      if (!rule->RebindCopy(opcodes_start, opcodes_size, buffer_end,
                            &data_size)) {
        return false;
      }
      size_t used = avail_size - data_size;
      buffer_end -= used;
      avail_size -= used;
      svc_opcode_count += op_count;
    }

    current_buffer->opcode_count = svc_opcode_count;
    size_t policy_buffers_occupied =
        (svc_opcode_count * sizeof(PolicyOpcode)) / sizeof(current_buffer[0]);
    current_buffer = &current_buffer[policy_buffers_occupied + 1];
  }

  return true;
}

PolicyRule::PolicyRule(EvalResult action) : action_(action), done_(false) {
  char* memory = new char[sizeof(PolicyBuffer) + kRuleBufferSize];
  buffer_ = reinterpret_cast<PolicyBuffer*>(memory);
  buffer_->opcode_count = 0;
  opcode_factory_ =
      new OpcodeFactory(buffer_, kRuleBufferSize + sizeof(PolicyOpcode));
}

PolicyRule::PolicyRule(const PolicyRule& other) {
  if (this == &other)
    return;
  action_ = other.action_;
  done_ = other.done_;
  size_t buffer_size = sizeof(PolicyBuffer) + kRuleBufferSize;
  char* memory = new char[buffer_size];
  buffer_ = reinterpret_cast<PolicyBuffer*>(memory);
  memcpy(buffer_, other.buffer_, buffer_size);

  char* opcode_buffer = reinterpret_cast<char*>(&buffer_->opcodes[0]);
  char* next_opcode = &opcode_buffer[GetOpcodeCount() * sizeof(PolicyOpcode)];
  opcode_factory_ =
      new OpcodeFactory(next_opcode, other.opcode_factory_->memory_size());
}

// This function get called from a simple state machine implemented in
// AddStringMatch() which passes the current state (in state) and it passes
// true in last_call if AddStringMatch() has finished processing the input
// pattern string and this would be the last call to generate any pending
// opcode. The skip_count is the currently accumulated number of '?' seen so
// far and once the associated opcode is generated this function sets it back
// to zero.
bool PolicyRule::GenStringOpcode(RuleType rule_type,
                                 StringMatchOptions match_opts,
                                 uint16_t parameter,
                                 int state,
                                 bool last_call,
                                 int* skip_count,
                                 std::wstring* fragment) {
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

  PolicyOpcode* op = nullptr;

  // The fragment string contains the accumulated characters to match with, it
  // never contains wildcards (unless they have been escaped) and while there
  // is no fragment there is no new string match opcode to generate.
  if (fragment->empty()) {
    // There is no new opcode to generate but in the last call we have to fix
    // the previous opcode because it was really the last but we did not know
    // it at that time.
    if (last_call && (buffer_->opcode_count > 0)) {
      op = &buffer_->opcodes[buffer_->opcode_count - 1];
      op->SetOptions(options);
    }
    return true;
  }

  if (PENDING_ASTERISK == state) {
    if (last_call) {
      op = opcode_factory_->MakeOpWStringMatch(parameter, fragment->c_str(),
                                               kSeekToEnd, match_opts, options);
    } else {
      op = opcode_factory_->MakeOpWStringMatch(
          parameter, fragment->c_str(), kSeekForward, match_opts, options);
    }

  } else if (PENDING_QMARK == state) {
    op = opcode_factory_->MakeOpWStringMatch(parameter, fragment->c_str(),
                                             *skip_count, match_opts, options);
    *skip_count = 0;
  } else {
    if (last_call) {
      match_opts = static_cast<StringMatchOptions>(EXACT_LENGTH | match_opts);
    }
    op = opcode_factory_->MakeOpWStringMatch(parameter, fragment->c_str(), 0,
                                             match_opts, options);
  }
  if (!op)
    return false;
  ++buffer_->opcode_count;
  fragment->clear();
  return true;
}

bool PolicyRule::AddStringMatch(RuleType rule_type,
                                int16_t parameter,
                                const wchar_t* string,
                                StringMatchOptions match_opts) {
  if (done_) {
    // Do not allow to add more rules after generating the action opcode.
    return false;
  }

  const wchar_t* current_char = string;
  uint32_t last_char = kLastCharIsNone;
  int state = PENDING_NONE;
  int skip_count = 0;       // counts how many '?' we have seen in a row.
  std::wstring fragment;    // accumulates the non-wildcard part.

  while (L'\0' != *current_char) {
    switch (*current_char) {
      case L'*':
        if (kLastCharIsWild & last_char) {
          // '**' and '&*' is an error.
          return false;
        }
        if (!GenStringOpcode(rule_type, match_opts, parameter, state, false,
                             &skip_count, &fragment)) {
          return false;
        }
        last_char = kLastCharIsAsterisk;
        state = PENDING_ASTERISK;
        break;
      case L'?':
        if (kLastCharIsAsterisk == last_char) {
          // '*?' is an error.
          return false;
        }
        if (!GenStringOpcode(rule_type, match_opts, parameter, state, false,
                             &skip_count, &fragment)) {
          return false;
        }
        ++skip_count;
        last_char = kLastCharIsQuestionM;
        state = PENDING_QMARK;
        break;
      case L'/':
        // Note: "/?" is an escaped '?'. Eat the slash and fall through.
        if (L'?' == current_char[1]) {
          ++current_char;
        }
        FALLTHROUGH;
      default:
        fragment += *current_char;
        last_char = kLastCharIsAlpha;
    }
    ++current_char;
  }

  if (!GenStringOpcode(rule_type, match_opts, parameter, state, true,
                       &skip_count, &fragment)) {
    return false;
  }
  return true;
}

bool PolicyRule::AddNumberMatch(RuleType rule_type,
                                int16_t parameter,
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
  ++buffer_->opcode_count;
  return true;
}

bool PolicyRule::Done() {
  if (done_) {
    return true;
  }
  if (!opcode_factory_->MakeOpAction(action_, kPolNone))
    return false;
  ++buffer_->opcode_count;
  done_ = true;
  return true;
}

bool PolicyRule::RebindCopy(PolicyOpcode* opcode_start,
                            size_t opcode_size,
                            char* data_start,
                            size_t* data_size) const {
  size_t count = buffer_->opcode_count;
  for (size_t ix = 0; ix != count; ++ix) {
    if (opcode_size < sizeof(PolicyOpcode)) {
      return false;
    }
    PolicyOpcode& opcode = buffer_->opcodes[ix];
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
      data_start -= str_len;
      memcpy(data_start, str, str_len);
      // Recompute the string displacement
      ptrdiff_t delta = data_start - reinterpret_cast<char*>(opcode_start);
      opcode_start->SetArgument(0, delta);
    }
    ++opcode_start;
    opcode_size -= sizeof(PolicyOpcode);
  }

  return true;
}

PolicyRule::~PolicyRule() {
  delete[] reinterpret_cast<char*>(buffer_);
  delete opcode_factory_;
}

}  // namespace sandbox
