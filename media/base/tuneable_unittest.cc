// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/tuneable.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class TuneableTest : public ::testing::Test {
 public:
  TuneableTest() = default;

  TuneableTest(const TuneableTest&) = delete;
  TuneableTest& operator=(const TuneableTest&) = delete;

  void SetUp() override {
    // Note that we might need to call value() to cache `tuneable_cached_` here.
    // We don't currently, since it's not needed.

    // Set everything else to non-default values.  We do this because we can't
    // do it in the tests without it doing odd things.
    // params[kTuneableIntSetToNot123] = "124";  // Not 123.
    // params[kTuneableInt0] = "0";
    // params[kTuneableInt5] = "5";
    // params[kTuneableInt10] = "10";

    // Set some tuneables to fixed values.
    SetFinchParameters(kTuneableIntSetToNot123, 124);
    SetFinchParameters(kTuneableInt0, 0);
    SetFinchParameters(kTuneableInt5, 5);
    SetFinchParameters(kTuneableInt10, 100);
    // TimeDelta should be given in milliseconds.
    SetFinchParameters(kTuneableTimeDeltaFiveSeconds, 5000);

    scoped_feature_list_.InitAndEnableFeatureWithParameters(kMediaOptimizer,
                                                            params_);
  }

  // Set the finch-chosen parameters for tuneable `name`.
  void SetFinchParameters(const char* name, int value) {
    params_[name] = base::NumberToString(value);
  }

  // Return the tuneable name for the `x`-th numbered tuneable.
  std::string GetNameForNumberedTuneable(const char* basename, int x) {
    std::string name(basename);
    base::AppendHexEncodedByte(static_cast<uint8_t>(x), name);
    return name;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::FieldTrialParams params_;

  // Params will set this to not 123.  We default it to 123 before Setup runs,
  // so we make sure that it uses the hardcoded defaults properly.
  static constexpr const char* kTuneableIntSetToNot123 = "t_int_not_123";
  Tuneable<int> tuneable_cached_{kTuneableIntSetToNot123, 0, 123, 200};

  static constexpr const char* kTuneableIntUnset = "t_int_unset";
  static constexpr const char* kTuneableInt0 = "t_int_0";
  static constexpr const char* kTuneableInt5 = "t_int_5";
  static constexpr const char* kTuneableInt10 = "t_int_10";
  static constexpr const char* kTuneableTimeDeltaFiveSeconds = "t_time_5s";
};

TEST_F(TuneableTest, IntTuneableCached) {
  // Verify that `tuneable_cached_` is, in fact, 123 even though the params try
  // to set it to something else.  This kind of, sort of, guarantees that it's
  // cached properly.
  EXPECT_EQ(tuneable_cached_.value(), 123);
}

TEST_F(TuneableTest, IntTuneableFromDefaultWithClamps) {
  // The default value should be used, and correctly clamped.
  constexpr int min_value = 0;
  constexpr int default_value = 4;
  constexpr int max_value = 10;
  Tuneable<int> t_min(kTuneableIntUnset, min_value, min_value - 1, max_value);
  EXPECT_EQ(t_min.value(), min_value);
  Tuneable<int> t_default(kTuneableIntUnset, min_value, default_value,
                          max_value);
  EXPECT_EQ(t_default.value(), default_value);
  Tuneable<int> t_max(kTuneableIntUnset, min_value, max_value + 1, max_value);
  EXPECT_EQ(t_max.value(), max_value);
}

TEST_F(TuneableTest, IntTuneableFromParams) {
  // Verify that params override the defaults, and are clamped correctly.
  constexpr int min_value = 1;
  constexpr int default_value = 4;  // That's not the same as the param.
  constexpr int max_value = 9;
  Tuneable<int> t_min(kTuneableInt0, min_value, default_value, max_value);
  EXPECT_EQ(t_min.value(), min_value);
  Tuneable<int> t_param(kTuneableInt5, min_value, default_value, max_value);
  EXPECT_EQ(t_param.value(), 5);
  Tuneable<int> t_max(kTuneableInt10, min_value, default_value, max_value);
  EXPECT_EQ(t_max.value(), max_value);
}

TEST_F(TuneableTest, OtherSpecializationsCompile) {
  // Since it's all templated, just be happy if it compiles and does something
  // somewhat sane.
  constexpr base::TimeDelta min_value = base::Seconds(0);
  constexpr base::TimeDelta default_value = base::Seconds(5);
  constexpr base::TimeDelta max_value = base::Seconds(10);
  Tuneable<base::TimeDelta> time_delta_tuneable("whatever", min_value,
                                                default_value, max_value);
  // Since the tuneable is not provided in the finch parameters, it should
  // equal the default.
  EXPECT_EQ(time_delta_tuneable.value(), default_value);

  Tuneable<size_t> size_t_tuneable("whatever_else", 0u, 100u, 500u);
  EXPECT_EQ(size_t_tuneable.value(), 100u);
}

TEST_F(TuneableTest, TimeDeltaIsSpecifiedInMilliseconds) {
  // Since the finch params are constructed with the assumption that the value
  // will be interpreted as milliseconds, make sure that the Tuneable actually
  // does interpret it that way.
  constexpr base::TimeDelta min_value = base::Seconds(0);
  constexpr base::TimeDelta max_value = base::Seconds(100);
  Tuneable<base::TimeDelta> t(kTuneableTimeDeltaFiveSeconds, min_value,
                              min_value, max_value);
  EXPECT_EQ(t.value(), base::Seconds(5));
}

}  // namespace media
