// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/cpu_probe_linux.h"

#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/compute_pressure/cpu_probe.h"
#include "content/browser/compute_pressure/cpuid_base_frequency_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class CpuProbeLinuxTest : public testing::Test {
 public:
  CpuProbeLinuxTest()
      : cpuid_base_frequency_(
            ParseBaseFrequencyFromCpuid(base::CPU().cpu_brand())) {}

  ~CpuProbeLinuxTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    fake_stat_path_ = temp_dir_.GetPath().AppendASCII("stat");
    // Create the /proc/stat file before creating the parser, in case the parser
    // implementation keeps an open handle to the file indefinitely.
    stat_file_ = base::File(fake_stat_path_, base::File::FLAG_CREATE_ALWAYS |
                                                 base::File::FLAG_WRITE);

    fake_cpufreq_root_path_ = temp_dir_.GetPath().AppendASCII("cpu");
    probe_ = CpuProbeLinux::CreateForTesting(
        fake_stat_path_, fake_cpufreq_root_path_.value().data());
  }

  bool WriteFakeStat(const std::string& contents) WARN_UNUSED_RESULT {
    if (!stat_file_.SetLength(0))
      return false;
    if (contents.size() > 0) {
      if (!stat_file_.Write(0, contents.data(), contents.size()))
        return false;
    }
    return true;
  }

  bool WriteFakeCpufreqFile(int core_id,
                            base::StringPiece file_name,
                            const std::string& contents) WARN_UNUSED_RESULT {
    DCHECK_GE(core_id, 0);

    base::FilePath core_root(
        base::StrCat({fake_cpufreq_root_path_.value(),
                      base::NumberToString(core_id), "/cpufreq"}));
    if (!base::CreateDirectory(core_root))
      return false;

    return base::WriteFile(core_root.AppendASCII(file_name), contents);
  }

  bool DeleteFakeCpufreqFile(int core_id,
                             base::StringPiece file_name) WARN_UNUSED_RESULT {
    DCHECK_GE(core_id, 0);

    base::FilePath core_root(
        base::StrCat({fake_cpufreq_root_path_.value(),
                      base::NumberToString(core_id), "/cpufreq"}));
    return base::DeleteFile(core_root.AppendASCII(file_name));
  }

  bool WriteFakeCpufreqCore(int core_id,
                            int64_t min_frequency,
                            int64_t max_frequency,
                            int64_t base_frequency,
                            int64_t current_frequency) WARN_UNUSED_RESULT {
    DCHECK_GE(core_id, 0);

    if (min_frequency >= 0) {
      if (!WriteFakeCpufreqFile(core_id, "cpuinfo_min_freq",
                                base::NumberToString(min_frequency))) {
        return false;
      }
    } else {
      if (!DeleteFakeCpufreqFile(core_id, "cpuinfo_min_freq"))
        return false;
    }

    if (max_frequency >= 0) {
      if (!WriteFakeCpufreqFile(core_id, "cpuinfo_max_freq",
                                base::NumberToString(max_frequency))) {
        return false;
      }
    } else {
      if (!DeleteFakeCpufreqFile(core_id, "cpuinfo_max_freq"))
        return false;
    }

    if (base_frequency >= 0) {
      if (!WriteFakeCpufreqFile(core_id, "base_frequency",
                                base::NumberToString(base_frequency))) {
        return false;
      }
    } else {
      if (!DeleteFakeCpufreqFile(core_id, "base_frequency"))
        return false;
    }

    if (current_frequency >= 0) {
      if (!WriteFakeCpufreqFile(core_id, "scaling_cur_freq",
                                base::NumberToString(current_frequency))) {
        return false;
      }
    } else {
      if (!DeleteFakeCpufreqFile(core_id, "scaling_cur_freq"))
        return false;
    }

    return true;
  }

 protected:
  const int64_t cpuid_base_frequency_;
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_stat_path_;
  base::FilePath fake_cpufreq_root_path_;
  base::File stat_file_;
  std::unique_ptr<CpuProbeLinux> probe_;
};

TEST_F(CpuProbeLinuxTest, ProductionData_NoCrash) {
  probe_->Update();
  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.0)
      << "No baseline on first Update()";
  EXPECT_EQ(probe_->LastSample().cpu_speed, 0.5)
      << "No baseline on first Update()";

  probe_->Update();
  EXPECT_GE(probe_->LastSample().cpu_utilization, 0.0);
  EXPECT_LE(probe_->LastSample().cpu_utilization, 1.0);
  EXPECT_GE(probe_->LastSample().cpu_speed, 0.0);
  EXPECT_LE(probe_->LastSample().cpu_speed, 1.0);
}

TEST_F(CpuProbeLinuxTest, OneCore_FullInfo) {
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 1'000'000'000));
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 3'400'000'000));
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.25);
  EXPECT_EQ(probe_->LastSample().cpu_speed, 0.75);
}

TEST_F(CpuProbeLinuxTest, TwoCores_FullInfo) {
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 1'000'000'000));
  ASSERT_TRUE(WriteFakeCpufreqCore(1, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 1'000'000'000));
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 3'400'000'000));
  ASSERT_TRUE(WriteFakeCpufreqCore(1, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 3'000'000'000));
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.375);
  EXPECT_EQ(probe_->LastSample().cpu_speed, 0.625);
}

TEST_F(CpuProbeLinuxTest, TwoCores_SecondCoreMissingCpufreq) {
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 1'000'000'000));
  ASSERT_TRUE(WriteFakeCpufreqCore(1, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 1'000'000'000));
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 3'400'000'000));
  ASSERT_TRUE(WriteFakeCpufreqCore(1, -1, -1, -1, -1));
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.375);
  EXPECT_EQ(probe_->LastSample().cpu_speed, 0.75);
}

TEST_F(CpuProbeLinuxTest, TwoCores_SecondCoreMissingStat) {
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 1'000'000'000));
  ASSERT_TRUE(WriteFakeCpufreqCore(1, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 1'000'000'000));
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
  ASSERT_TRUE(WriteFakeCpufreqCore(0, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 3'400'000'000));
  ASSERT_TRUE(WriteFakeCpufreqCore(1, 1'000'000'000, 3'800'000'000,
                                   3'000'000'000, 3'000'000'000));
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.25);
  EXPECT_EQ(probe_->LastSample().cpu_speed, 0.75);
}

TEST_F(CpuProbeLinuxTest, OneCore_NoCpufreqBaseFrequency) {
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

  SCOPED_TRACE(cpuid_base_frequency_);
  if (cpuid_base_frequency_ >= 0) {
    ASSERT_TRUE(WriteFakeCpufreqCore(
        0, 0, cpuid_base_frequency_ + 1'000'000'000, -1, 0));
  } else {
    ASSERT_TRUE(WriteFakeCpufreqCore(0, 0, 1'000'000'000, -1, 0));
  }
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

  if (cpuid_base_frequency_ >= 0) {
    ASSERT_TRUE(WriteFakeCpufreqCore(0, 0,
                                     cpuid_base_frequency_ + 1'000'000'000, -1,
                                     cpuid_base_frequency_ + 250'000'000));
  } else {
    ASSERT_TRUE(WriteFakeCpufreqCore(0, 0, 1'000'000'000, -1, 250'000'000));
  }
  probe_->Update();

  EXPECT_EQ(probe_->LastSample().cpu_utilization, 0.25);
  EXPECT_EQ(probe_->LastSample().cpu_speed, 0.625);
}

}  // namespace content
