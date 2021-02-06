// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_POLICY_LOW_LEVEL_H__
#define SANDBOX_SRC_POLICY_LOW_LEVEL_H__

#include <stddef.h>
#include <stdint.h>

#include <list>

#include <string>

#include "base/macros.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_engine_params.h"

// Low level policy classes.
// Built on top of the PolicyOpcode and OpcodeFactory, the low level policy
// provides a way to define rules on strings and numbers but it is unaware
// of Windows specific details or how the Interceptions must be set up.
// To use these classes you construct one or more rules and add them to the
// LowLevelPolicy object like this:
//
//   PolicyRule rule1(ASK_BROKER);
//   rule1.AddStringMatch(IF, 0, L"\\\\/?/?\\c:\\*Microsoft*\\*.exe", true);
//   rule1.AddNumberMatch(IF_NOT, 1, CREATE_ALWAYS, EQUAL);
//   rule1.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_NORMAL, EQUAL);
//
//   PolicyRule rule2(FAKE_SUCCESS);
//   rule2.AddStringMatch(IF, 0, L"\\\\/?/?\\Pipe\\Chrome.*", false));
//   rule2.AddNumberMatch(IF, 1, OPEN_EXISTING, EQUAL));
//
//   LowLevelPolicy policyGen(*policy_memory);
//   policyGen.AddRule(kNtCreateFileSvc, &rule1);
//   policyGen.AddRule(kNtCreateFileSvc, &rule2);
//   policyGen.Done();
//
// At this point (error checking omitted) the policy_memory can be copied
// to the target process where it can be evaluated.

namespace sandbox {

// Defines the memory layout of the policy. This memory is filled by
// LowLevelPolicy object.
// For example:
//
//  [Service 0] --points to---\
//  [Service 1] --------------|-----\
//   ......                   |     |
//  [Service N]               |     |
//  [data_size]               |     |
//  [Policy Buffer 0] <-------/     |
//  [opcodes of]                    |
//  .......                         |
//  [Policy Buffer 1] <-------------/
//  [opcodes]
//  .......
//  .......
//  [Policy Buffer N]
//  [opcodes]
//  .......
//   <possibly unused space here>
//  .......
//  [opcode string ]
//  [opcode string ]
//  .......
//  [opcode string ]
struct PolicyGlobal {
  PolicyBuffer* entry[kMaxServiceCount];
  size_t data_size;
  PolicyBuffer data[1];
};

class PolicyRule;

// Provides the means to collect rules into a policy store (memory)
class LowLevelPolicy {
 public:
  // policy_store: must contain allocated memory and the internal
  // size fields set to correct values.
  explicit LowLevelPolicy(PolicyGlobal* policy_store);

  // Destroys all the policy rules.
  ~LowLevelPolicy();

  // Adds a rule to be generated when Done() is called.
  // service: The id of the service that this rule is associated with,
  // for example the 'Open Thread' service or the "Create File" service.
  // returns false on error.
  bool AddRule(IpcTag service, PolicyRule* rule);

  // Generates all the rules added with AddRule() into the memory area
  // passed on the constructor. Returns false on error.
  bool Done();

 private:
  struct RuleNode {
    const PolicyRule* rule;
    IpcTag service;
  };
  std::list<RuleNode> rules_;
  PolicyGlobal* policy_store_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(LowLevelPolicy);
};

// There are 'if' rules and 'if not' comparisons
enum RuleType {
  IF = 0,
  IF_NOT = 1,
};

// Possible comparisons for numbers
enum RuleOp {
  EQUAL,
  AND,
  RANGE  // TODO(cpu): Implement this option.
};

// Provides the means to collect a set of comparisons into a single
// rule and its associated action.
class PolicyRule {
  friend class LowLevelPolicy;

 public:
  explicit PolicyRule(EvalResult action);
  PolicyRule(const PolicyRule& other);
  ~PolicyRule();

  // Adds a string comparison to the rule.
  // rule_type: possible values are IF and IF_NOT.
  // parameter: the expected index of the argument for this rule. For example
  // in a 'create file' service the file name argument can be at index 0.
  // string: is the desired matching pattern.
  // match_opts: if the pattern matching is case sensitive or not.
  bool AddStringMatch(RuleType rule_type,
                      int16_t parameter,
                      const wchar_t* string,
                      StringMatchOptions match_opts);

  // Adds a number match comparison to the rule.
  // rule_type: possible values are IF and IF_NOT.
  // parameter: the expected index of the argument for this rule.
  // number: the value to compare the input to.
  // comparison_op: the comparison kind (equal, logical and, etc).
  bool AddNumberMatch(RuleType rule_type,
                      int16_t parameter,
                      uint32_t number,
                      RuleOp comparison_op);

  // Returns the number of opcodes generated so far.
  size_t GetOpcodeCount() const { return buffer_->opcode_count; }

  // Called when there is no more comparisons to add. Internally it generates
  // the last opcode (the action opcode). Returns false if this operation fails.
  bool Done();

 private:
  void operator=(const PolicyRule&);
  // Called in a loop from AddStringMatch to generate the required string
  // match opcodes. rule_type, match_opts and parameter are the same as
  // in AddStringMatch.
  bool GenStringOpcode(RuleType rule_type,
                       StringMatchOptions match_opts,
                       uint16_t parameter,
                       int state,
                       bool last_call,
                       int* skip_count,
                       std::wstring* fragment);

  // Loop over all generated opcodes and copy them to increasing memory
  // addresses from opcode_start and copy the extra data (strings usually) into
  // decreasing addresses from data_start. Extra data is only present in the
  // string evaluation opcodes.
  bool RebindCopy(PolicyOpcode* opcode_start,
                  size_t opcode_size,
                  char* data_start,
                  size_t* data_size) const;
  PolicyBuffer* buffer_;
  OpcodeFactory* opcode_factory_;
  EvalResult action_;
  bool done_;
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_POLICY_LOW_LEVEL_H__
