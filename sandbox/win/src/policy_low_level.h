// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SANDBOX_WIN_SRC_POLICY_LOW_LEVEL_H_
#define SANDBOX_WIN_SRC_POLICY_LOW_LEVEL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>

#include <string>

#include "base/memory/raw_ptr.h"
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
  // Returns true if the IPC for `service` should be registered for the target.
  // Should only be called after Done() has been called to finalize the setup.
  bool NeedsIpc(IpcTag service) {
    return entry[static_cast<size_t>(service)] != nullptr;
  }

  PolicyBuffer* entry[kSandboxIpcCount];
  size_t data_size;
  PolicyBuffer data[1];
};

class PolicyRule;

// Provides the means to collect rules into a policy store (memory)
class LowLevelPolicy {
 public:
  LowLevelPolicy() = delete;

  // policy_store: must contain allocated memory and the internal
  // size fields set to correct values.
  explicit LowLevelPolicy(PolicyGlobal* policy_store);

  LowLevelPolicy(const LowLevelPolicy&) = delete;
  LowLevelPolicy& operator=(const LowLevelPolicy&) = delete;

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
    raw_ptr<const PolicyRule, DanglingUntriaged> rule;
    IpcTag service;
  };
  std::list<RuleNode> rules_;
  raw_ptr<PolicyGlobal, DanglingUntriaged> policy_store_;
};

// There are 'if' rules and 'if not' comparisons
enum RuleType {
  IF = 0,
  IF_NOT = 1,
};

// Possible comparisons for numbers
enum RuleOp { EQUAL, AND };

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
  bool AddStringMatch(RuleType rule_type,
                      uint8_t parameter,
                      const wchar_t* string);

  // Adds a number match comparison to the rule.
  // rule_type: possible values are IF and IF_NOT.
  // parameter: the expected index of the argument for this rule.
  // number: the value to compare the input to.
  // comparison_op: the comparison kind (equal, logical and, etc).
  bool AddNumberMatch(RuleType rule_type,
                      uint8_t parameter,
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
  // match opcodes. rule_type and parameter are the same as in AddStringMatch.
  bool GenStringOpcode(RuleType rule_type,
                       uint8_t parameter,
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
  raw_ptr<PolicyBuffer> buffer_;
  raw_ptr<OpcodeFactory> opcode_factory_;
  EvalResult action_;
  bool done_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_POLICY_LOW_LEVEL_H_
