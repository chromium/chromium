// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"
#include "courgette/image_utils.h"
#include "courgette/streams.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace courgette {

namespace {

class AdjustmentMethodTest : public testing::Test {
 public:
  void Test1() const;

 private:
  void SetUp() {
  }

  void TearDown() {
  }

  // Returns one of two similar simple programs. These differ only in Label
  // assignment, so it is possible to make them look identical.
  std::unique_ptr<AssemblyProgram> MakeProgram(int kind) const {
    auto prog = std::make_unique<AssemblyProgram>(EXE_WIN_32_X86, 0x00400000);

    RVA kRvaA = 0x00410000;
    RVA kRvaB = 0x00410004;

    std::vector<RVA> abs32_rvas;
    abs32_rvas.push_back(kRvaA);
    abs32_rvas.push_back(kRvaB);
    std::vector<RVA> rel32_rvas;  // Stub.

    TrivialRvaVisitor abs32_visitor(abs32_rvas);
    TrivialRvaVisitor rel32_visitor(rel32_rvas);
    prog->PrecomputeLabels(&abs32_visitor, &rel32_visitor);

    Label* labelA = prog->FindAbs32Label(kRvaA);
    Label* labelB = prog->FindAbs32Label(kRvaB);

    InstructionGenerator gen = base::BindRepeating(
        [](Label* labelA, Label* labelB,
           InstructionReceptor* receptor) -> CheckBool {
          EXPECT_TRUE(receptor->EmitAbs32(labelA));
          EXPECT_TRUE(receptor->EmitAbs32(labelA));
          EXPECT_TRUE(receptor->EmitAbs32(labelB));
          EXPECT_TRUE(receptor->EmitAbs32(labelA));
          EXPECT_TRUE(receptor->EmitAbs32(labelA));
          EXPECT_TRUE(receptor->EmitAbs32(labelB));
          return true;
        },
        labelA, labelB);

    EXPECT_TRUE(prog->AnnotateLabels(gen));
    EXPECT_EQ(6U, prog->abs32_label_annotations().size());
    EXPECT_EQ(0U, prog->rel32_label_annotations().size());

    if (kind == 0) {
      labelA->index_ = 0;
      labelB->index_ = 1;
    } else {
      labelA->index_ = 1;
      labelB->index_ = 0;
    }
    prog->AssignRemainingIndexes();

    return prog;
  }

  std::unique_ptr<AssemblyProgram> MakeProgramA() const {
    return MakeProgram(0);
  }
  std::unique_ptr<AssemblyProgram> MakeProgramB() const {
    return MakeProgram(1);
  }

  // Returns a string that is the serialized version of |program| annotations.
  std::string Serialize(AssemblyProgram* program) const {
    std::ostringstream oss;
    for (const Label* label : program->abs32_label_annotations())
      oss << "(" << label->rva_ << "," << label->index_ << ")";
    oss << ";";
    for (const Label* label : program->rel32_label_annotations())
      oss << "(" << label->rva_ << "," << label->index_ << ")";

    EXPECT_GT(oss.str().length(), 1U);  // Ensure results are non-trivial.
    return oss.str();
  }
};

void AdjustmentMethodTest::Test1() const {
  std::unique_ptr<AssemblyProgram> prog1 = MakeProgramA();
  std::unique_ptr<AssemblyProgram> prog2 = MakeProgramB();
  std::string s1 = Serialize(prog1.get());
  std::string s2 = Serialize(prog2.get());

  // Don't use EXPECT_EQ because strings are unprintable.
  EXPECT_FALSE(s1 == s2);  // Unadjusted A and B differ.

  std::unique_ptr<AssemblyProgram> prog5 = MakeProgramA();
  std::unique_ptr<AssemblyProgram> prog6 = MakeProgramB();
  Status can_adjust = Adjust(*prog5, prog6.get());
  EXPECT_EQ(C_OK, can_adjust);
  std::string s5 = Serialize(prog5.get());
  std::string s6 = Serialize(prog6.get());

  EXPECT_TRUE(s1 == s5);  // Adjustment did not change A (prog5)
  EXPECT_TRUE(s5 == s6);  // Adjustment did change B into A
}

TEST_F(AdjustmentMethodTest, All) {
  Test1();
}

}  // namespace

}  // namespace courgette
