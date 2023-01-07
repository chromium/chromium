// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff/bsdiff.h"

#include <stddef.h>

#include "courgette/base_test_unittest.h"
#include "courgette/courgette.h"
#include "courgette/streams.h"

class BSDiffMemoryTest : public BaseTest {
 public:
  void GenerateAndTestPatch(const std::string& a, const std::string& b) const;

  std::string GenerateSyntheticInput(size_t length, int seed) const;
};

void BSDiffMemoryTest::GenerateAndTestPatch(const std::string& old_text,
                                            const std::string& new_text) const {
  courgette::SourceStream old1;
  courgette::SourceStream new1;
  old1.Init(old_text.c_str(), old_text.length());
  new1.Init(new_text.c_str(), new_text.length());

  courgette::SinkStream patch1;
  bsdiff::BSDiffStatus status =
      bsdiff::CreateBinaryPatch(&old1, &new1, &patch1);
  EXPECT_EQ(bsdiff::OK, status);

  courgette::SourceStream old2;
  courgette::SourceStream patch2;
  old2.Init(old_text.c_str(), old_text.length());
  patch2.Init(patch1);

  courgette::SinkStream new2;
  status = bsdiff::ApplyBinaryPatch(&old2, &patch2, &new2);
  EXPECT_EQ(bsdiff::OK, status);
  EXPECT_EQ(new_text.length(), new2.Length());
  EXPECT_EQ(0, memcmp(new_text.c_str(), new2.Buffer(), new_text.length()));
}

std::string BSDiffMemoryTest::GenerateSyntheticInput(size_t length, int seed)
  const {
  static const char* a[8] = {"O", "A", "x", "-", "y", ".", "|", ":"};
  std::string result;
  while (result.length() < length) {
    seed = (seed + 17) * 1049 + (seed >> 27);
    result.append(a[seed & 7]);
  }
  result.resize(length);
  return result;
}

TEST_F(BSDiffMemoryTest, TestEmpty) {
  GenerateAndTestPatch(std::string(), std::string());
}

TEST_F(BSDiffMemoryTest, TestEmptyVsNonempty) {
  GenerateAndTestPatch(std::string(), "xxx");
}

TEST_F(BSDiffMemoryTest, TestNonemptyVsEmpty) {
  GenerateAndTestPatch("xxx", std::string());
}

TEST_F(BSDiffMemoryTest, TestSmallInputsWithSmallChanges) {
  std::string file1 =
      "I would not, could not, in a box.\n"
      "I could not, would not, with a fox.\n"
      "I will not eat them with a mouse.\n"
      "I will not eat them in a house.\n"
      "I will not eat them here or there.\n"
      "I will not eat them anywhere.\n"
      "I do not eat green eggs and ham.\n"
      "I do not like them, Sam-I-am.\n";
  std::string file2 =
      "I would not, could not, in a BOX.\n"
      "I could not, would not, with a FOX.\n"
      "I will not eat them with a MOUSE.\n"
      "I will not eat them in a HOUSE.\n"
      "I will not eat them in a HOUSE.\n"     // Extra line.
      "I will not eat them here or THERE.\n"
      "I will not eat them ANYWHERE.\n"
      "I do not eat green eggs and HAM.\n"
      "I do not like them, Sam-I-am.\n";
  GenerateAndTestPatch(file1, file2);
}

TEST_F(BSDiffMemoryTest, TestNearPageArrayPageSize) {
  // This magic number is the size of one block of the PageArray in
  // third_party/bsdiff_create.cc.
  size_t critical_size = 1 << 18;

  // Test first-inputs with sizes that straddle the magic size to test this
  // PageArray's internal boundary condition.

  std::string file1 = GenerateSyntheticInput(critical_size, 0);
  std::string file2 = GenerateSyntheticInput(critical_size, 1);
  GenerateAndTestPatch(file1, file2);

  std::string file1a = file1.substr(0, critical_size - 1);
  GenerateAndTestPatch(file1a, file2);

  std::string file1b = file1.substr(0, critical_size - 2);
  GenerateAndTestPatch(file1b, file2);

  std::string file1c = file1 + file1.substr(0, 1);
  GenerateAndTestPatch(file1c, file2);
}

TEST_F(BSDiffMemoryTest, TestIndenticalDlls) {
  std::string file1 = FileContents("en-US.dll");
  GenerateAndTestPatch(file1, file1);
}

TEST_F(BSDiffMemoryTest, TestDifferentExes) {
  std::string file1 = FileContents("setup1.exe");
  std::string file2 = FileContents("setup2.exe");
  GenerateAndTestPatch(file1, file2);
}

TEST_F(BSDiffMemoryTest, TestDifferentElfs) {
  std::string file1 = FileContents("elf-32-1");
  std::string file2 = FileContents("elf-32-2");
  GenerateAndTestPatch(file1, file2);
}
