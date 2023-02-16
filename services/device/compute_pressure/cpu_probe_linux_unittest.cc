// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_linux.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class CpuProbeLinuxTest : public testing::Test {
 public:
  // Frequency value passed to WriteFakeCpufreqCore() meaning "delete the file".
  static constexpr int64_t kDeleteFakeFile = -1;

  CpuProbeLinuxTest() = default;

  ~CpuProbeLinuxTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    fake_stat_path_ = temp_dir_.GetPath().AppendASCII("stat");
    // Create the /proc/stat file before creating the parser, in case the parser
    // implementation keeps an open handle to the file indefinitely.
    stat_file_ = base::File(fake_stat_path_, base::File::FLAG_CREATE_ALWAYS |
                                                 base::File::FLAG_WRITE);

    probe_ = CpuProbeLinux::CreateForTesting(fake_stat_path_);
  }

  [[nodiscard]] bool WriteFakeStat(const std::string& contents) {
    if (!stat_file_.SetLength(0))
      return false;
    if (contents.size() > 0) {
      if (!stat_file_.Write(0, contents.data(), contents.size()))
        return false;
    }
    return true;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_stat_path_;
  base::File stat_file_;
  std::unique_ptr<CpuProbeLinux> probe_;
};

TEST_F(CpuProbeLinuxTest, ProductionDataNoCrash) {
  probe_->Update();
  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.0)
      << "No baseline on first Update()";

  probe_->Update();
  EXPECT_GE(probe_->LastSample().cpu_utilization, 0.0);
  EXPECT_LE(probe_->LastSample().cpu_utilization, 1.0);
}

TEST_F(CpuProbeLinuxTest, OneCoreFullInfo) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 0 0 0 0 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  probe_->Update();

  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 100 0 0 300 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.25);
}

TEST_F(CpuProbeLinuxTest, TwoCoresFullInfo) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 0 0 0 0 0 0 0 0 0 0
cpu1 0 0 0 0 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  probe_->Update();

  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 100 0 0 300 0 0 0 0 0 0
cpu1 100 100 0 200 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.375);
}

TEST_F(CpuProbeLinuxTest, TwoCoresSecondCoreMissingStat) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 0 0 0 0 0 0 0 0 0 0
cpu1 0 0 0 0 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  probe_->Update();

  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 100 0 0 300 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.25);
}

}  // namespace device
