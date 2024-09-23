// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/policy_low_level.h"

#include <stddef.h>
#include <stdint.h>

#include "sandbox/win/src/policy_engine_params.h"
#include "sandbox/win/src/policy_engine_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

#define POLPARAMS_BEGIN(x) sandbox::ParameterSet x[] = {
#define POLPARAM(p) sandbox::ParamPickerMake(p),
#define POLPARAMS_END }

namespace sandbox {

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
}

// Helper function to allocate space (on the heap) for policy.
PolicyGlobal* MakePolicyMemory() {
  const size_t kTotalPolicySz = 4096 * 8;
  char* mem = new char[kTotalPolicySz];
  memset(mem, 0, kTotalPolicySz);
  PolicyGlobal* policy = reinterpret_cast<PolicyGlobal*>(mem);
  policy->data_size = kTotalPolicySz - sizeof(PolicyGlobal);
  return policy;
}

// The simplest test using LowLevelPolicy it should test a single opcode which
// does a exact string comparison.
TEST(PolicyEngineTest, SimpleStrMatch) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"z:\\Directory\\domo.txt"));

  PolicyGlobal* policy = MakePolicyMemory();
  const IpcTag kFakeService = IpcTag::PING2;

  LowLevelPolicy policyGen(policy);
  EXPECT_TRUE(policyGen.AddRule(kFakeService, &pr));
  EXPECT_TRUE(policyGen.Done());

  const wchar_t* filename = L"Z:\\Directory\\domo.txt";

  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"Z:\\Directory\\domo.txt.tmp";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  delete[] reinterpret_cast<char*>(policy);
}

TEST(PolicyEngineTest, SimpleIfNotStrMatch) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\"));

  PolicyGlobal* policy = MakePolicyMemory();
  const IpcTag kFakeService = IpcTag::PING2;
  LowLevelPolicy policyGen(policy);

  EXPECT_TRUE(policyGen.AddRule(kFakeService, &pr));
  EXPECT_TRUE(policyGen.Done());

  const wchar_t* filename = nullptr;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Microsoft\\domo.txt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  delete[] reinterpret_cast<char*>(policy);
}

TEST(PolicyEngineTest, SimpleIfNotStrMatchWild1) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\*"));

  PolicyGlobal* policy = MakePolicyMemory();
  const IpcTag kFakeService = IpcTag::NTCREATEFILE;
  LowLevelPolicy policyGen(policy);

  EXPECT_TRUE(policyGen.AddRule(kFakeService, &pr));
  EXPECT_TRUE(policyGen.Done());

  const wchar_t* filename = nullptr;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\domo.txt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\domo.txt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  delete[] reinterpret_cast<char*>(policy);
}

TEST(PolicyEngineTest, SimpleIfNotStrMatchWild2) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\*.txt"));

  PolicyGlobal* policy = MakePolicyMemory();
  const IpcTag kFakeService = IpcTag::NTCREATEFILE;
  LowLevelPolicy policyGen(policy);

  EXPECT_TRUE(policyGen.AddRule(kFakeService, &pr));
  EXPECT_TRUE(policyGen.Done());

  const wchar_t* filename = nullptr;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\domo.txt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\domo.txt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Microsoft\\domo.bmp";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  delete[] reinterpret_cast<char*>(policy);
}

TEST(PolicyEngineTest, IfNotStrMatchTwoRulesWild1) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF_NOT, 0, L"c:\\Microsoft\\*"));
  EXPECT_TRUE(pr.AddNumberMatch(IF, 1, 24, EQUAL));

  PolicyGlobal* policy = MakePolicyMemory();
  const IpcTag kFakeService = IpcTag::NTCREATEFILE;
  LowLevelPolicy policyGen(policy);

  EXPECT_TRUE(policyGen.AddRule(kFakeService, &pr));
  EXPECT_TRUE(policyGen.Done());

  const wchar_t* filename = nullptr;
  uint32_t access = 0;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(filename)  // Argument 0
    POLPARAM(access)    // Argument 1
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev(policy->entry[static_cast<size_t>(kFakeService)]);

  filename = L"c:\\Microsoft\\domo.txt";
  access = 24;
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\Microsoft\\domo.txt";
  access = 42;
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\MicroNerd\\domo.txt";
  access = 24;
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Micronesia\\domo.txt";
  access = 42;
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  delete[] reinterpret_cast<char*>(policy);
}

// Testing one single rule in one single service. The service is made to
// resemble NtCreateFile.
TEST(PolicyEngineTest, OneRuleTest) {
  PolicyRule pr(ASK_BROKER);
  EXPECT_TRUE(pr.AddStringMatch(IF, 0, L"c:\\*Microsoft*\\*.txt"));
  EXPECT_TRUE(pr.AddNumberMatch(IF_NOT, 1, CREATE_ALWAYS, EQUAL));
  EXPECT_TRUE(pr.AddNumberMatch(IF, 2, FILE_ATTRIBUTE_NORMAL, EQUAL));

  PolicyGlobal* policy = MakePolicyMemory();

  const IpcTag kNtFakeCreateFile = IpcTag::NTCREATEFILE;

  LowLevelPolicy policyGen(policy);
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, &pr));
  EXPECT_TRUE(policyGen.Done());

  const wchar_t* filename = L"c:\\Documents and Settings\\Microsoft\\BLAH.txt";
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

  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  creation_mode = CREATE_ALWAYS;
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  creation_mode = OPEN_EXISTING;
  filename = L"c:\\Other\\Path\\Microsoft\\Another file.txt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Other\\Path\\Microsoft\\Another file.txt.tmp";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  flags = FILE_ATTRIBUTE_DEVICE;
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\Other\\Macrosoft\\Another file.txt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"c:\\Microsoft\\1.txt";
  flags = FILE_ATTRIBUTE_NORMAL;
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev.GetAction());

  filename = L"c:\\Microsoft\\1.ttt";
  result = pol_ev.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  delete[] reinterpret_cast<char*>(policy);
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

  PolicyGlobal* policy = MakePolicyMemory();

  // These do not match the real tag values.
  const IpcTag kNtFakeNone = static_cast<IpcTag>(4);
  const IpcTag kNtFakeCreateFile = static_cast<IpcTag>(5);
  const IpcTag kNtFakeOpenFile = static_cast<IpcTag>(6);

  LowLevelPolicy policyGen(policy);
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, &pr_pipe));
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, &pr_dump));
  EXPECT_TRUE(policyGen.AddRule(kNtFakeCreateFile, &pr_winexe));

  EXPECT_TRUE(policyGen.AddRule(kNtFakeOpenFile, &pr_pipe));

  EXPECT_TRUE(policyGen.AddRule(kNtFakeNone, &pr_none));

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
  EXPECT_EQ(OP_ACTION, policy->entry[static_cast<size_t>(kNtFakeNone)]
                           ->opcodes[tc1 - 1]
                           .GetID());
  EXPECT_EQ(OP_WSTRING_MATCH,
            policy->entry[static_cast<size_t>(kNtFakeCreateFile)]
                ->opcodes[0]
                .GetID());
  EXPECT_EQ(OP_ACTION, policy->entry[static_cast<size_t>(kNtFakeCreateFile)]
                           ->opcodes[tc2 - 1]
                           .GetID());
  EXPECT_EQ(
      OP_WSTRING_MATCH,
      policy->entry[static_cast<size_t>(kNtFakeOpenFile)]->opcodes[0].GetID());
  EXPECT_EQ(OP_ACTION, policy->entry[static_cast<size_t>(kNtFakeOpenFile)]
                           ->opcodes[tc3 - 1]
                           .GetID());

  // Test the policy evaluation.

  const wchar_t* filename = L"";
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

  result = eval_CreateFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_None.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"\\\\??\\c:\\Windows\\System32\\calc.exe";
  flags = FILE_ATTRIBUTE_SYSTEM;
  result = eval_CreateFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_None.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  flags += FILE_ATTRIBUTE_READONLY;
  result = eval_CreateFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_None.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(FAKE_SUCCESS, eval_None.GetAction());
  result = eval_OpenFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  flags = FILE_ATTRIBUTE_NORMAL;
  result = eval_CreateFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(SIGNAL_ALARM, eval_CreateFile.GetAction());
  result = eval_None.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"\\\\??\\c:\\some path\\other path\\crash reports\\some path";
  creation_mode = CREATE_ALWAYS;
  result = eval_CreateFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, eval_CreateFile.GetAction());
  result = eval_None.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  filename = L"\\\\??\\Pipe\\Chrome.12345";
  creation_mode = OPEN_EXISTING;
  result = eval_CreateFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(FAKE_SUCCESS, eval_CreateFile.GetAction());
  result = eval_None.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(NO_POLICY_MATCH, result);
  result = eval_OpenFile.Evaluate(kShortEval, params, _countof(params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(FAKE_SUCCESS, eval_OpenFile.GetAction());

  delete[] reinterpret_cast<char*>(policy);
}

TEST(PolicyEngineTest, PolicyRuleCopyConstructorTwoStrings) {
  // Both pr_orig and pr_copy should allow hello.* but not *.txt files.
  PolicyRule pr_orig(ASK_BROKER);
  EXPECT_TRUE(pr_orig.AddStringMatch(IF, 0, L"hello.*"));

  PolicyRule pr_copy(pr_orig);
  EXPECT_TRUE(pr_orig.AddStringMatch(IF_NOT, 0, L"*.txt"));
  EXPECT_TRUE(pr_copy.AddStringMatch(IF_NOT, 0, L"*.txt"));

  PolicyGlobal* policy = MakePolicyMemory();
  LowLevelPolicy policyGen(policy);
  EXPECT_TRUE(policyGen.AddRule(IpcTag::PING1, &pr_orig));
  EXPECT_TRUE(policyGen.AddRule(IpcTag::PING2, &pr_copy));
  EXPECT_TRUE(policyGen.Done());

  const wchar_t* name = nullptr;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(name)
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev_orig(policy->entry[1]);
  name = L"domo.txt";
  result = pol_ev_orig.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  name = L"hello.bmp";
  result = pol_ev_orig.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev_orig.GetAction());

  PolicyProcessor pol_ev_copy(policy->entry[2]);
  name = L"domo.txt";
  result = pol_ev_copy.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  name = L"hello.bmp";
  result = pol_ev_copy.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev_copy.GetAction());
}

TEST(PolicyEngineTest, PolicyGenDoneCalledTwice) {
  // The specific rules here are not important.
  PolicyRule pr_orig(ASK_BROKER);
  EXPECT_TRUE(pr_orig.AddStringMatch(IF, 0, L"hello.*"));

  PolicyRule pr_copy(pr_orig);
  EXPECT_TRUE(pr_orig.AddStringMatch(IF_NOT, 0, L"*.txt"));
  EXPECT_TRUE(pr_copy.AddStringMatch(IF_NOT, 0, L"*.txt"));

  PolicyGlobal* policy = MakePolicyMemory();
  LowLevelPolicy policyGen(policy);
  const IpcTag tag1 = IpcTag::PING1;
  const IpcTag tag2 = IpcTag::PING2;
  EXPECT_TRUE(policyGen.AddRule(tag1, &pr_orig));
  EXPECT_TRUE(policyGen.AddRule(tag2, &pr_copy));
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
  const wchar_t* name = nullptr;
  POLPARAMS_BEGIN(eval_params)
    POLPARAM(name)
  POLPARAMS_END;

  PolicyResult result;
  PolicyProcessor pol_ev_orig(policy->entry[1]);
  name = L"domo.txt";
  result = pol_ev_orig.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(NO_POLICY_MATCH, result);

  name = L"hello.bmp";
  result = pol_ev_orig.Evaluate(kShortEval, eval_params, _countof(eval_params));
  EXPECT_EQ(POLICY_MATCH, result);
  EXPECT_EQ(ASK_BROKER, pol_ev_orig.GetAction());
}

}  // namespace sandbox
