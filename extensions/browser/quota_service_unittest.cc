// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/quota_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;

namespace extensions {

using Bucket = QuotaLimitHeuristic::Bucket;
using Config = QuotaLimitHeuristic::Config;
using BucketList = QuotaLimitHeuristic::BucketList;
using TimedLimit = QuotaService::TimedLimit;

namespace {

const char kGenericName[] = "name";
const Config kFrozenConfig = {0, base::Days(0)};
const Config k2PerMinute = {2, base::Minutes(1)};
const TimeTicks kStartTime = TimeTicks();
const TimeTicks k1MinuteAfterStart = kStartTime + base::Minutes(1);

class Mapper : public QuotaLimitHeuristic::BucketMapper {
 public:
  Mapper() {}

  Mapper(const Mapper&) = delete;
  Mapper& operator=(const Mapper&) = delete;

  ~Mapper() override = default;
  void GetBucketsForArgs(const base::Value::List& args,
                         BucketList* buckets) override {
    for (const auto& val : args) {
      std::optional<int> id = val.GetIfInt();
      ASSERT_TRUE(id.has_value());
      auto& entry = buckets_[*id];
      if (!entry) {
        entry = std::make_unique<Bucket>();
      }
      buckets->push_back(entry.get());
    }
  }

 private:
  std::map<int, std::unique_ptr<Bucket>> buckets_;
};

class MockMapper : public QuotaLimitHeuristic::BucketMapper {
 public:
  void GetBucketsForArgs(const base::Value::List& args,
                         BucketList* buckets) override {}
};

class MockFunction : public ExtensionFunction {
 public:
  explicit MockFunction(const char* name) { SetName(name); }

  ResponseAction Run() override { return RespondLater(); }

 protected:
  ~MockFunction() override {}
};

class TimedLimitMockFunction : public MockFunction {
 public:
  explicit TimedLimitMockFunction(const char* name) : MockFunction(name) {}
  void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const override {
    heuristics->push_back(std::make_unique<TimedLimit>(
        k2PerMinute, std::make_unique<Mapper>(), kGenericName));
  }

 private:
  ~TimedLimitMockFunction() override {}
};

class FrozenMockFunction : public MockFunction {
 public:
  explicit FrozenMockFunction(const char* name) : MockFunction(name) {}
  void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const override {
    heuristics->push_back(std::make_unique<TimedLimit>(
        kFrozenConfig, std::make_unique<Mapper>(), kGenericName));
  }

 private:
  ~FrozenMockFunction() override {}
};
}  // namespace

class QuotaServiceTest : public testing::Test {
 public:
  QuotaServiceTest()
      : extension_a_("a"), extension_b_("b"), extension_c_("c") {}
  void SetUp() override { service_ = std::make_unique<QuotaService>(); }
  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    service_.reset();
  }

 protected:
  std::string extension_a_;
  std::string extension_b_;
  std::string extension_c_;
  std::unique_ptr<QuotaService> service_;
  content::BrowserTaskEnvironment task_environment_;
};

class QuotaLimitHeuristicTest : public testing::Test {
 public:
  static void DoMoreThan2PerMinuteFor5Minutes(const TimeTicks& start_time,
                                              QuotaLimitHeuristic* lim,
                                              Bucket* b,
                                              int an_unexhausted_minute) {
    for (int i = 0; i < 5; i++) {
      // Perform one operation in each minute.
      int m = i * 60;
      EXPECT_TRUE(lim->Apply(b, start_time + base::Seconds(10 + m)));
      EXPECT_TRUE(b->has_tokens());

      if (i == an_unexhausted_minute) {
        continue;  // Don't exhaust all tokens this minute.
      }

      EXPECT_TRUE(lim->Apply(b, start_time + base::Seconds(15 + m)));
      EXPECT_FALSE(b->has_tokens());

      // These are OK because we haven't exhausted all buckets.
      EXPECT_TRUE(lim->Apply(b, start_time + base::Seconds(20 + m)));
      EXPECT_FALSE(b->has_tokens());
      EXPECT_TRUE(lim->Apply(b, start_time + base::Seconds(50 + m)));
      EXPECT_FALSE(b->has_tokens());
    }
  }
};

TEST_F(QuotaLimitHeuristicTest, Timed) {
  TimedLimit lim(k2PerMinute, std::make_unique<MockMapper>(), kGenericName);
  Bucket b;

  b.Reset(k2PerMinute, kStartTime);
  EXPECT_TRUE(lim.Apply(&b, kStartTime));
  EXPECT_TRUE(b.has_tokens());
  EXPECT_TRUE(lim.Apply(&b, kStartTime + base::Seconds(30)));
  EXPECT_FALSE(b.has_tokens());
  EXPECT_FALSE(lim.Apply(&b, k1MinuteAfterStart));

  b.Reset(k2PerMinute, kStartTime);
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart - base::Seconds(1)));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart + base::Seconds(1)));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart + base::Seconds(2)));
  EXPECT_FALSE(lim.Apply(&b, k1MinuteAfterStart + base::Seconds(3)));
}

TEST_F(QuotaServiceTest, NoHeuristic) {
  scoped_refptr<MockFunction> f(new MockFunction("foo"));
  base::Value::List args;
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), args, kStartTime));
}

TEST_F(QuotaServiceTest, FrozenHeuristic) {
  scoped_refptr<MockFunction> f(new FrozenMockFunction("foo"));
  base::Value::List args;
  args.Append(1);
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), args, kStartTime));
}

TEST_F(QuotaServiceTest, SingleHeuristic) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  base::Value::List args;
  args.Append(1);
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), args, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), args,
                                 kStartTime + base::Seconds(10)));
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), args,
                                 kStartTime + base::Seconds(15)));

  base::Value::List args2;
  args2.Append(1);
  args2.Append(2);
  EXPECT_EQ("", service_->Assess(extension_b_, f.get(), args2, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_b_, f.get(), args2,
                                 kStartTime + base::Seconds(10)));

  base::TimeDelta peace = base::Minutes(30);
  EXPECT_EQ("",
            service_->Assess(extension_b_, f.get(), args, kStartTime + peace));
  EXPECT_EQ("", service_->Assess(extension_b_, f.get(), args,
                                 kStartTime + peace + base::Seconds(10)));
  EXPECT_NE("", service_->Assess(extension_b_, f.get(), args2,
                                 kStartTime + peace + base::Seconds(15)));

  // Test that items are independent.
  base::Value::List args3;
  args3.Append(3);
  EXPECT_EQ("", service_->Assess(extension_c_, f.get(), args, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_c_, f.get(), args3,
                                 kStartTime + base::Seconds(10)));
  EXPECT_EQ("", service_->Assess(extension_c_, f.get(), args,
                                 kStartTime + base::Seconds(15)));
  EXPECT_EQ("", service_->Assess(extension_c_, f.get(), args3,
                                 kStartTime + base::Seconds(20)));
  EXPECT_NE("", service_->Assess(extension_c_, f.get(), args,
                                 kStartTime + base::Seconds(25)));
  EXPECT_NE("", service_->Assess(extension_c_, f.get(), args3,
                                 kStartTime + base::Seconds(30)));
}

TEST_F(QuotaServiceTest, MultipleFunctionsDontInterfere) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  scoped_refptr<MockFunction> g(new TimedLimitMockFunction("bar"));

  base::Value::List args_f;
  base::Value::List args_g;
  args_f.Append(1);
  args_g.Append(2);

  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), args_f, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_a_, g.get(), args_g, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), args_f,
                                 kStartTime + base::Seconds(10)));
  EXPECT_EQ("", service_->Assess(extension_a_, g.get(), args_g,
                                 kStartTime + base::Seconds(10)));
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), args_f,
                                 kStartTime + base::Seconds(15)));
  EXPECT_NE("", service_->Assess(extension_a_, g.get(), args_g,
                                 kStartTime + base::Seconds(15)));
}

TEST_F(QuotaServiceTest, ViolatorsWillBeForgiven) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  base::Value::List arg;
  arg.Append(1);
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), arg, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), arg,
                                 kStartTime + base::Seconds(10)));
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), arg,
                                 kStartTime + base::Seconds(15)));

  // Waiting a while will give the extension access to the function again.
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), arg,
                                 kStartTime + base::Days(1)));

  // And lose it again soon after.
  EXPECT_EQ("",
            service_->Assess(extension_a_, f.get(), arg,
                             kStartTime + base::Days(1) + base::Seconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_a_, f.get(), arg,
                             kStartTime + base::Days(1) + base::Seconds(15)));

  // Going further over quota should continue to fail within this time period,
  // but still all restored later.
  EXPECT_NE("",
            service_->Assess(extension_a_, f.get(), arg,
                             kStartTime + base::Days(1) + base::Seconds(20)));
  EXPECT_NE("",
            service_->Assess(extension_a_, f.get(), arg,
                             kStartTime + base::Days(1) + base::Seconds(25)));

  // Like now.
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), arg,
                                 kStartTime + base::Days(2)));
}

}  // namespace extensions
