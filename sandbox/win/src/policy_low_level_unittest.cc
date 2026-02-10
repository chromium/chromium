// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/policy_low_level.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

#define POLPARAMS_BEGIN(x) sandbox::ParameterSet x[] = {
#define POLPARAM(p) sandbox::ParamPickerMake(p),
#define POLPARAMS_END }

namespace sandbox {

namespace {

class PolicyGlobalBuffer {
 public:
  PolicyGlobalBuffer(const size_t size = 4096 * 8) : buffer_(size) {
    get()->data_size = size - sizeof(PolicyGlobal);
  }
  PolicyGlobal* get() {
    return reinterpret_cast<PolicyGlobal*>(std::data(buffer_));
  }
  PolicyGlobal* operator->() { return get(); }

 private:
  std::vector<char> buffer_;
};

}  // namespace

// Testing that we allow opcode generation on valid string patterns.
TEST(PolicyEngineTest, StringPatternsOK) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"c:\\adobe\\ver??\\"));
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"*.tmp"));
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"c:\\*.doc"));
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"c:\\windows\\*"));
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"d:\\adobe\\acrobat.exe"));
}

// Testing that we signal invalid string patterns.
TEST(PolicyEngineTest, StringPatternsBAD) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_FALSE(pr.AddStringMatch(IF, 0, L"one**two"));
  EXPECT_FALSE(pr.AddStringMatch(IF, 0, L"**three"));
  EXPECT_FALSE(pr.AddStringMatch(IF, 0, L""));
}

// The simplest test using LowLevelPolicy it should test a single opcode which
// does a exact string comparison.
TEST(PolicyEngineTest, SimpleStrMatch) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"z:\\Directory\\domo.txt"));

  PolicyGlobalBuffer policy;
  const IpcTag kFakeService = IpcTag::PING2;

  LowLevelPolicy policyGen(policy.get());
  EXPECT_TRUE(policyGen.AddRule(kFakeService, std::move(pr)));
  EXPECT_TRUE(policyGen.Done());

  std::wstring_view filename = L"Z:\\Directory\\domo.txt";

  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"Z:\\Directory\\domo.txt.tmp";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
}

TEST(PolicyEngineTest, SimpleIfNotStrMatch) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\"));

  PolicyGlobalBuffer policy;
  const IpcTag kFakeService = IpcTag::PING2;
  LowLevelPolicy policyGen(policy.get());

  EXPECT_TRUE(policyGen.AddRule(kFakeService, std::move(pr)));
  EXPECT_TRUE(policyGen.Done());

  std::wstring_view filename;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Microsoft\\domo.txt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());
}

TEST(PolicyEngineTest, SimpleIfNotStrMatchWild1) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\*"));

  PolicyGlobalBuffer policy;
  const IpcTag kFakeService = IpcTag::NTCREATEFILE;
  LowLevelPolicy policyGen(policy.get());

  EXPECT_TRUE(policyGen.AddRule(kFakeService, std::move(pr)));
  EXPECT_TRUE(policyGen.Done());

  std::wstring_view filename;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\domo.txt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\domo.txt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());
}

TEST(PolicyEngineTest, SimpleIfNotStrMatchWild2) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\*.txt"));

  PolicyGlobalBuffer policy;
  const IpcTag kFakeService = IpcTag::NTCREATEFILE;
  LowLevelPolicy policyGen(policy.get());

  EXPECT_TRUE(policyGen.AddRule(kFakeService, std::move(pr)));
  EXPECT_TRUE(policyGen.Done());

  std::wstring_view filename;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\domo.txt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\domo.txt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Microsoft\\domo.bmp";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());
}

TEST(PolicyEngineTest, IfNotStrMatchTwoRulesWild1) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\*"));
  EXPECT_TRUE(pr.AddNumberMatch(IF, 1, 24, EQUAL));

  PolicyGlobalBuffer policy;
  const IpcTag kFakeService = IpcTag::NTCREATEFILE;
  LowLevelPolicy policyGen(policy.get());

  EXPECT_TRUE(policyGen.AddRule(kFakeService, std::move(pr)));
  EXPECT_TRUE(policyGen.Done());

  std::wstring_view filename;
  uint32_t access = 0;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
    POLPARAM(access)    // Argument 1
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\domo.txt";
  access = 24;
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\Microsoft\\domo.txt";
  access = 42;
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\domo.txt";
  access = 24;
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Micronesia\\domo.txt";
  access = 42;
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
}

// Testing one single rule in one single service. The service is made to
// resemble NtCreateFile.
TEST(PolicyEngineTest, OneRuleTest) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"c:\\*Microsoft*\\*.txt"));
  EXPECT_TRUE(pr.AddNumberMatch(IF_NOT, 1, CREATE_ALWAYS, EQUAL));
  EXPECT_TRUE(pr.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_NORMAL, EQUAL));

  PolicyGlobalBuffer policy;

  const IpcTag kNtFakeCreateFile = IpcTag::NTCREATEFILE;

  LowLevelPolicy policyGen(policy.get());
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, std::move(pr)));
  EXPECT_TRUE(policyGen.Done());

  std::wstring_view filename =
      L"c:\\Documents and Settings\\Microsoft\\BLAH.txt";
  uint32_t creation_mode = OPEN_EXISTING;
  uint32_t flags = FILE_ATTRIBUTE_NORMAL;
  void* security_descriptor = nullptr;

  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)       // Argument 0
    POLPARAM(creation_mode)  // Argument 1
    POLPARAM(flags)          // Argument 2
    POLPARAM(security_descriptor)
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kNtFakeCreateFile)]);

  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  creation_mode = CREATE_ALWAYS;
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  creation_mode = OPEN_EXISTING;
  filename = L"c:\\Other\\Path\\Microsoft\\Another file.txt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Other\\Path\\Microsoft\\Another file.txt.tmp";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  flags = FILE_ATTRIBUTE_DEVICE;
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\Other\\Macrosoft\\Another file.txt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\Microsoft\\1.txt";
  flags = FILE_ATTRIBUTE_NORMAL;
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Microsoft\\1.ttt";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
}

// Testing 3 rules in 3 services. Two of the services resemble File services.
TEST(PolicyEngineTest, ThreeRulesTest) {
  PolicyRule pr_pipe(FAKE_SUCCESS);
  EXPECT_TRUE(pr_pipe.AddStringMatch(IF, 0, L"\\\\??\\Pipe\\Chrome.*"));
  EXPECT_TRUE(pr_pipe.AddNumberMatch(IF, 1, OPEN_EXISTING, EQUAL));
  EXPECT_TRUE(pr_pipe.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_NORMAL, EQUAL));

  size_t opc1 = pr_pipe.GetOpcodeCount();
  EXPECT_EQ(3u, opc1);

  PolicyRule pr_dump(ASK_BROKER);
  EXPECT_TRUE(pr_dump.AddStringMatch(IF, 0, L"\\\\??\\*\\Crash Reports\\*"));
  EXPECT_TRUE(pr_dump.AddNumberMatch(IF, 1, CREATE_ALWAYS, EQUAL));
  EXPECT_TRUE(pr_dump.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_NORMAL, EQUAL));

  size_t opc2 = pr_dump.GetOpcodeCount();
  EXPECT_EQ(4u, opc2);

  PolicyRule pr_winexe(SIGNAL_ALARM);
  EXPECT_TRUE(pr_winexe.AddStringMatch(IF, 0, L"\\\\??\\C:\\Windows\\*.exe"));
  EXPECT_TRUE(pr_winexe.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_NORMAL, EQUAL));

  size_t opc3 = pr_winexe.GetOpcodeCount();
  EXPECT_EQ(3u, opc3);

  PolicyRule pr_none(FAKE_SUCCESS);
  EXPECT_TRUE(pr_none.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_READONLY, AND));
  EXPECT_TRUE(pr_none.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_SYSTEM, AND));

  size_t opc4 = pr_none.GetOpcodeCount();
  EXPECT_EQ(2u, opc4);

  PolicyRule pr_open(FAKE_SUCCESS);
  EXPECT_TRUE(pr_open.AddStringMatch(IF, 0, L"\\\\??\\Pipe\\Chrome.*"));
  EXPECT_TRUE(pr_open.AddNumberMatch(IF, 1, OPEN_EXISTING, EQUAL));
  EXPECT_TRUE(pr_open.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_NORMAL, EQUAL));

  size_t opc5 = pr_open.GetOpcodeCount();
  EXPECT_EQ(3u, opc5);

  PolicyGlobalBuffer policy;

  // These do not match the real tag values.
  const IpcTag kNtFakeNone = static_cast<IpcTag>(4);
  const IpcTag kNtFakeCreateFile = static_cast<IpcTag>(5);
  const IpcTag kNtFakeOpenFile = static_cast<IpcTag>(6);

  LowLevelPolicy policyGen(policy.get());
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, std::move(pr_pipe)));
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, std::move(pr_dump)));
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, std::move(pr_winexe)));

  EXPECT_TRUE(policyGen.AddRule(kNtFakeOpenFile, std::move(pr_open)));

  EXPECT_TRUE(policyGen.AddRule(kNtFakeNone, std::move(pr_none)));

  EXPECT_TRUE(policyGen.Done());

  // Inspect the policy structure manually.
  EXPECT_FALSE(policy->entry[0]);
  EXPECT_FALSE(policy->entry[1]);
  EXPECT_FALSE(policy->entry[2]);
  EXPECT_FALSE(policy->entry[3]);
  EXPECT_TRUE(policy->entry[4]);  // kNtFakeNone.
  EXPECT_TRUE(policy->entry[5]);  // kNtFakeCreateFile.
  EXPECT_TRUE(policy->entry[6]);  // kNtFakeOpenFile.
  EXPECT_FALSE(policy->entry[7]);

  // The total per service opcode counts now must take in account one
  // extra opcode (action opcode) per rule.
  ++opc1;
  ++opc2;
  ++opc3;
  ++opc4;

  size_t tc1 = policy->entry[static_cast<size_t>(kNtFakeNone)]->opcode_count;
  size_t tc2 =
      policy->entry[static_cast<size_t>(kNtFakeCreateFile)]->opcode_count;
  size_t tc3 =
      policy->entry[static_cast<size_t>(kNtFakeOpenFile)]->opcode_count;

  EXPECT_EQ(opc4, tc1);
  EXPECT_EQ((opc1 + opc2 + opc3), tc2);
  EXPECT_EQ(opc1, tc3);

  // Check the type of the first and last opcode of each service.

  EXPECT_EQ(
      OP_NUMBER_AND_MATCH,
      policy->entry[static_cast<size_t>(kNtFakeNone)]->opcodes[0].GetID());
  EXPECT_EQ(OP_ACTION,
            UNSAFE_TODO(policy->entry[static_cast<size_t>(kNtFakeNone)])
                ->opcodes[tc1 - 1]
                .GetID());
  EXPECT_EQ(OP_WSTRING_MATCH,
            policy->entry[static_cast<size_t>(kNtFakeCreateFile)]
                ->opcodes[0]
                .GetID());
  EXPECT_EQ(OP_ACTION,
            UNSAFE_TODO(policy->entry[static_cast<size_t>(kNtFakeCreateFile)])
                ->opcodes[tc2 - 1]
                .GetID());
  EXPECT_EQ(
      OP_WSTRING_MATCH,
      policy->entry[static_cast<size_t>(kNtFakeOpenFile)]->opcodes[0].GetID());
  EXPECT_EQ(OP_ACTION,
            UNSAFE_TODO(policy->entry[static_cast<size_t>(kNtFakeOpenFile)])
                ->opcodes[tc3 - 1]
                .GetID());

  // Test the policy evaluation.

  std::wstring_view filename = L"";
  uint32_t creation_mode = OPEN_EXISTING;
  uint32_t flags = FILE_ATTRIBUTE_NORMAL;
  void* security_descriptor = nullptr;

  POLPARAMS_BEGIN(params)
    POLPARAM(filename)       // Argument 0
    POLPARAM(creation_mode)  // Argument 1
    POLPARAM(flags)          // Argument 2
    POLPARAM(security_descriptor)
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor eval_CreateFile(
      policy->entry[static_cast<size_t>(kNtFakeCreateFile)]);
  PolicyProcessor eval_OpenFile(
      policy->entry[static_cast<size_t>(kNtFakeOpenFile)]);
  PolicyProcessor eval_None(policy->entry[static_cast<size_t>(kNtFakeNone)]);

  result = eval_CreateFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_None.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"\\\\??\\c:\\Windows\\System32\\calc.exe";
  flags = FILE_ATTRIBUTE_SYSTEM;
  result = eval_CreateFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_None.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  flags += FILE_ATTRIBUTE_READONLY;
  result = eval_CreateFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_None.Evaluate(params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(FAKE_SUCCESS, eval_None.GetAction());
  result = eval_OpenFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  flags = FILE_ATTRIBUTE_NORMAL;
  result = eval_CreateFile.Evaluate(params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(SIGNAL_ALARM, eval_CreateFile.GetAction());
  result = eval_None.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"\\\\??\\c:\\some path\\other path\\crash reports\\some path";
  creation_mode = CREATE_ALWAYS;
  result = eval_CreateFile.Evaluate(params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, eval_CreateFile.GetAction());
  result = eval_None.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"\\\\??\\Pipe\\Chrome.12345";
  creation_mode = OPEN_EXISTING;
  result = eval_CreateFile.Evaluate(params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(FAKE_SUCCESS, eval_CreateFile.GetAction());
  result = eval_None.Evaluate(params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(FAKE_SUCCESS, eval_OpenFile.GetAction());
}

TEST(PolicyEngineTest, PolicyGenDoneCalledTwice) {
  // The specific rules here are not important.
  PolicyRule pr1(ASK_BROKER);
  PolicyRule pr2(ASK_BROKER);
  EXPECT_TRUE(pr1.AddStringMatch(IF, 0, L"hello.*"));
  EXPECT_TRUE(pr2.AddStringMatch(IF, 0, L"hello.*"));
  EXPECT_TRUE(pr1.AddStringMatch(IF_NOT, 0, L"*.txt"));
  EXPECT_TRUE(pr2.AddStringMatch(IF_NOT, 0, L"*.txt"));

  PolicyGlobalBuffer policy;
  LowLevelPolicy policyGen(policy.get());
  const IpcTag tag1 = IpcTag::PING1;
  const IpcTag tag2 = IpcTag::PING2;
  EXPECT_TRUE(policyGen.AddRule(tag1, std::move(pr1)));
  EXPECT_TRUE(policyGen.AddRule(tag2, std::move(pr2)));
  EXPECT_TRUE(policyGen.Done());

  // Obtain opcode counts.
  size_t tc1 = policy->entry[static_cast<size_t>(IpcTag::PING1)]->opcode_count;
  size_t tc2 = policy->entry[static_cast<size_t>(IpcTag::PING2)]->opcode_count;

  // Call Done() again.
  EXPECT_TRUE(policyGen.Done());

  // Expect same opcode counts.
  EXPECT_EQ(tc1,
            policy->entry[static_cast<size_t>(IpcTag::PING1)]->opcode_count);
  EXPECT_EQ(tc2,
            policy->entry[static_cast<size_t>(IpcTag::PING2)]->opcode_count);

  // Confirm the rules work as before.
  std::wstring_view name;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(name)
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev_orig(policy->entry[1]);
  name = L"domo.txt";
  result = pol_ev_orig.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  name = L"hello.bmp";
  result = pol_ev_orig.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev_orig.GetAction());
}

TEST(PolicyEngineTest, ReturnConst) {
  constexpr uintptr_t kConstantValue = static_cast<uintptr_t>(-1);
  PolicyRule pr(RETURN_CONST, kConstantValue);
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"ABC"));

  PolicyGlobalBuffer policy;
  const IpcTag kFakeService = IpcTag::PING2;

  LowLevelPolicy policyGen(policy.get());
  EXPECT_TRUE(policyGen.AddRule(kFakeService, std::move(pr)));
  EXPECT_TRUE(policyGen.Done());

  std::wstring_view filename = L"ABC";

  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(RETURN_CONST, pol_ev.GetAction());
  EXPECT_EQ(kConstantValue, pol_ev.GetConstant());

  filename = L"XYZ";
  result = pol_ev.Evaluate(eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
}

}  // namespace sandbox
