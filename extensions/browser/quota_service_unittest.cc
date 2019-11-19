// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/quota_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace extensions {

using Bucket = QuotaLimitHeuristic::Bucket;
using Config = QuotaLimitHeuristic::Config;
using BucketList = QuotaLimitHeuristic::BucketList;
using TimedLimit = QuotaService::TimedLimit;

namespace {

const char kGenericName[] = "name";
const Config kFrozenConfig = {0, TimeDelta::FromDays(0)};
const Config k2PerMinute = {2, TimeDelta::FromMinutes(1)};
const TimeTicks kStartTime = TimeTicks();
const TimeTicks k1MinuteAfterStart = kStartTime + TimeDelta::FromMinutes(1);

class Mapper : public QuotaLimitHeuristic::BucketMapper {
 public:
  Mapper() {}
  ~Mapper() override {}
  void GetBucketsForArgs(const base::ListValue* args,
                         BucketList* buckets) override {
    for (size_t i = 0; i < args->GetSize(); i++) {
      int id;
      ASSERT_TRUE(args->GetInteger(i, &id));
      if (buckets_.find(id) == buckets_.end())
        buckets_[id] = std::make_unique<Bucket>();
      buckets->push_back(buckets_[id].get());
    }
  }

 private:
  std::map<int, std::unique_ptr<Bucket>> buckets_;
  DISALLOW_COPY_AND_ASSIGN(Mapper);
};

class MockMapper : public QuotaLimitHeuristic::BucketMapper {
 public:
  void GetBucketsForArgs(const base::ListValue* args,
                         BucketList* buckets) override {}
};

class MockFunction : public ExtensionFunction {
 public:
  explicit MockFunction(const char* name) { set_name(name); }

  ResponseAction Run() override { return RespondLater(); }

 protected:
  ~MockFunction() override {}
};

class TimedLimitMockFunction : public MockFunction {
 public:
  explicit TimedLimitMockFunction(const char* name) : MockFunction(name) {}
  void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const override {
    heuristics->push_back(
        std::make_unique<TimedLimit>(k2PerMinute, new Mapper(), kGenericName));
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
        kFrozenConfig, new Mapper(), kGenericName));
  }

 private:
  ~FrozenMockFunction() override {}
};
}  // namespace

class QuotaServiceTest : public testing::Test {
 public:
  QuotaServiceTest()
      : extension_a_("a"), extension_b_("b"), extension_c_("c") {}
  void SetUp() override { service_.reset(new QuotaService()); }
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
      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(10 + m)));
      EXPECT_TRUE(b->has_tokens());

      if (i == an_unexhausted_minute)
        continue;  // Don't exhaust all tokens this minute.

      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(15 + m)));
      EXPECT_FALSE(b->has_tokens());

      // These are OK because we haven't exhausted all buckets.
      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(20 + m)));
      EXPECT_FALSE(b->has_tokens());
      EXPECT_TRUE(lim->Apply(b, start_time + TimeDelta::FromSeconds(50 + m)));
      EXPECT_FALSE(b->has_tokens());
    }
  }
};

TEST_F(QuotaLimitHeuristicTest, Timed) {
  TimedLimit lim(k2PerMinute, new MockMapper(), kGenericName);
  Bucket b;

  b.Reset(k2PerMinute, kStartTime);
  EXPECT_TRUE(lim.Apply(&b, kStartTime));
  EXPECT_TRUE(b.has_tokens());
  EXPECT_TRUE(lim.Apply(&b, kStartTime + TimeDelta::FromSeconds(30)));
  EXPECT_FALSE(b.has_tokens());
  EXPECT_FALSE(lim.Apply(&b, k1MinuteAfterStart));

  b.Reset(k2PerMinute, kStartTime);
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart - TimeDelta::FromSeconds(1)));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart + TimeDelta::FromSeconds(1)));
  EXPECT_TRUE(lim.Apply(&b, k1MinuteAfterStart + TimeDelta::FromSeconds(2)));
  EXPECT_FALSE(lim.Apply(&b, k1MinuteAfterStart + TimeDelta::FromSeconds(3)));
}

TEST_F(QuotaServiceTest, NoHeuristic) {
  scoped_refptr<MockFunction> f(new MockFunction("foo"));
  base::ListValue args;
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &args, kStartTime));
}

TEST_F(QuotaServiceTest, FrozenHeuristic) {
  scoped_refptr<MockFunction> f(new FrozenMockFunction("foo"));
  base::ListValue args;
  args.AppendInteger(1);
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), &args, kStartTime));
}

TEST_F(QuotaServiceTest, SingleHeuristic) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  base::ListValue args;
  args.AppendInteger(1);
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &args, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(15)));

  base::ListValue args2;
  args2.AppendInteger(1);
  args2.AppendInteger(2);
  EXPECT_EQ("", service_->Assess(extension_b_, f.get(), &args2, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_b_,
                             f.get(),
                             &args2,
                             kStartTime + TimeDelta::FromSeconds(10)));

  TimeDelta peace = TimeDelta::FromMinutes(30);
  EXPECT_EQ("",
            service_->Assess(extension_b_, f.get(), &args, kStartTime + peace));
  EXPECT_EQ("",
            service_->Assess(extension_b_,
                             f.get(),
                             &args,
                             kStartTime + peace + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_b_,
                             f.get(),
                             &args2,
                             kStartTime + peace + TimeDelta::FromSeconds(15)));

  // Test that items are independent.
  base::ListValue args3;
  args3.AppendInteger(3);
  EXPECT_EQ("", service_->Assess(extension_c_, f.get(), &args, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args3,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_EQ("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(15)));
  EXPECT_EQ("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args3,
                             kStartTime + TimeDelta::FromSeconds(20)));
  EXPECT_NE("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args,
                             kStartTime + TimeDelta::FromSeconds(25)));
  EXPECT_NE("",
            service_->Assess(extension_c_,
                             f.get(),
                             &args3,
                             kStartTime + TimeDelta::FromSeconds(30)));
}

TEST_F(QuotaServiceTest, MultipleFunctionsDontInterfere) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  scoped_refptr<MockFunction> g(new TimedLimitMockFunction("bar"));

  base::ListValue args_f;
  base::ListValue args_g;
  args_f.AppendInteger(1);
  args_g.AppendInteger(2);

  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &args_f, kStartTime));
  EXPECT_EQ("", service_->Assess(extension_a_, g.get(), &args_g, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args_f,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             g.get(),
                             &args_g,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             f.get(),
                             &args_f,
                             kStartTime + TimeDelta::FromSeconds(15)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             g.get(),
                             &args_g,
                             kStartTime + TimeDelta::FromSeconds(15)));
}

TEST_F(QuotaServiceTest, ViolatorsWillBeForgiven) {
  scoped_refptr<MockFunction> f(new TimedLimitMockFunction("foo"));
  base::ListValue arg;
  arg.AppendInteger(1);
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &arg, kStartTime));
  EXPECT_EQ("",
            service_->Assess(extension_a_,
                             f.get(),
                             &arg,
                             kStartTime + TimeDelta::FromSeconds(10)));
  EXPECT_NE("",
            service_->Assess(extension_a_,
                             f.get(),
                             &arg,
                             kStartTime + TimeDelta::FromSeconds(15)));

  // Waiting a while will give the extension access to the function again.
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &arg,
                                 kStartTime + TimeDelta::FromDays(1)));

  // And lose it again soon after.
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &arg,
                                 kStartTime + TimeDelta::FromDays(1) +
                                     TimeDelta::FromSeconds(10)));
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), &arg,
                                 kStartTime + TimeDelta::FromDays(1) +
                                     TimeDelta::FromSeconds(15)));

  // Going further over quota should continue to fail within this time period,
  // but still all restored later.
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), &arg,
                                 kStartTime + TimeDelta::FromDays(1) +
                                     TimeDelta::FromSeconds(20)));
  EXPECT_NE("", service_->Assess(extension_a_, f.get(), &arg,
                                 kStartTime + TimeDelta::FromDays(1) +
                                     TimeDelta::FromSeconds(25)));

  // Like now.
  EXPECT_EQ("", service_->Assess(extension_a_, f.get(), &arg,
                                 kStartTime + TimeDelta::FromDays(2)));
}

}  // namespace extensions
