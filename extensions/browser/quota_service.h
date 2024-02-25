// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The QuotaService uses heuristics to limit abusive requests
// made by extensions.  In this model 'items' (e.g individual bookmarks) are
// represented by a 'Bucket' that holds state for that item for one single
// interval of time.  The interval of time is defined as 'how long we need to
// watch an item (for a particular heuristic) before making a decision about
// quota violations'.  A heuristic is two functions: one mapping input
// arguments to a unique Bucket (the BucketMapper), and another to determine
// if a new request involving such an item at a given time is a violation.

#ifndef EXTENSIONS_BROWSER_QUOTA_SERVICE_H_
#define EXTENSIONS_BROWSER_QUOTA_SERVICE_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

class ExtensionFunction;

namespace extensions {
class QuotaLimitHeuristic;

using QuotaLimitHeuristics = std::list<std::unique_ptr<QuotaLimitHeuristic>>;

// The QuotaService takes care that calls to certain extension
// functions do not exceed predefined quotas.
//
// The QuotaService needs to live entirely on one thread, i.e. be created,
// called and destroyed on the same thread, due to its use of a RepeatingTimer.
// It is not a KeyedService because instances exist on both the UI
// and IO threads.
class QuotaService {
 public:
  // Some concrete heuristics (declared below) that ExtensionFunctions can
  // use to help the service make decisions about quota violations.
  class TimedLimit;

  QuotaService();

  QuotaService(const QuotaService&) = delete;
  QuotaService& operator=(const QuotaService&) = delete;

  virtual ~QuotaService();

  // Decide whether the invocation of |function| with argument |args| by the
  // extension specified by |extension_id| results in a quota limit violation.
  // Returns an error message representing the failure if quota was exceeded,
  // or empty-string if the request is fine and can proceed.
  // |args| must be a list.
  std::string Assess(const ExtensionId& extension_id,
                     ExtensionFunction* function,
                     const base::Value::List& args,
                     const base::TimeTicks& event_time);

  // An active ScopedDisablePurgeForTesting prevents QuotaService's constructor
  // from starting a purge timer.
  class ScopedDisablePurgeForTesting {
   public:
    ScopedDisablePurgeForTesting();

    ScopedDisablePurgeForTesting(const ScopedDisablePurgeForTesting&) = delete;
    ScopedDisablePurgeForTesting& operator=(
        const ScopedDisablePurgeForTesting&) = delete;

    ~ScopedDisablePurgeForTesting();
  };

 private:
  using FunctionName = std::string;
  // All QuotaLimitHeuristic instances in this map are owned by us.
  using FunctionHeuristicsMap = std::map<FunctionName, QuotaLimitHeuristics>;

  // Purge resets all accumulated data as if the service was just created.
  // Called periodically so we don't consume an unbounded amount of memory
  // while tracking quota.
  void Purge();
  base::RepeatingTimer purge_timer_;

  // Our quota tracking state for extensions that have invoked quota limited
  // functions.  Each extension is treated separately, so extension ids are the
  // key for the mapping.  As an extension invokes functions, the map keeps
  // track of which functions it has invoked and the heuristics for each one.
  // Each heuristic will be evaluated and ANDed together to get a final answer.
  std::map<ExtensionId, FunctionHeuristicsMap> function_heuristics_;

  THREAD_CHECKER(thread_checker_);
};

// A QuotaLimitHeuristic is two things: 1, A heuristic to map extension
// function arguments to corresponding Buckets for each input arg, and 2) a
// heuristic for determining if a new event involving a particular item
// (represented by its Bucket) constitutes a quota violation.
class QuotaLimitHeuristic {
 public:
  // Parameters to configure the amount of tokens allotted to individual
  // Bucket objects (see Below) and how often they are replenished.
  struct Config {
    // The maximum number of tokens a bucket can contain, and is refilled to
    // every epoch.
    int64_t refill_token_count;

    // Specifies how frequently the bucket is logically refilled with tokens.
    base::TimeDelta refill_interval;
  };

  // A Bucket is how the heuristic portrays an individual item (since quota
  // limits are per item) and all associated state for an item that needs to
  // carry through multiple calls to Apply.  It "holds" tokens, which are
  // debited and credited in response to new events involving the item being
  // being represented.  For convenience, instead of actually periodically
  // refilling buckets they are just 'Reset' on-demand (e.g. when new events
  // come in). So, a bucket has an expiration to denote it has becomes stale.
  class Bucket {
   public:
    Bucket() : num_tokens_(0) {}

    Bucket(const Bucket&) = delete;
    Bucket& operator=(const Bucket&) = delete;

    // Removes a token from this bucket, and returns true if the bucket had
    // any tokens in the first place.
    bool DeductToken() { return num_tokens_-- > 0; }

    // Returns true if this bucket has tokens to deduct.
    bool has_tokens() const { return num_tokens_ > 0; }

    // Reset this bucket to specification (from internal configuration), to be
    // valid from |start| until the first refill interval elapses and it needs
    // to be reset again.
    void Reset(const Config& config, const base::TimeTicks& start);

    // The time at which the token count and next expiration should be reset,
    // via a call to Reset.
    const base::TimeTicks& expiration() { return expiration_; }

   private:
    base::TimeTicks expiration_;
    int64_t num_tokens_;
  };
  using BucketList = std::list<Bucket*>;

  // A helper interface to retrieve the bucket corresponding to |args| from
  // the set of buckets (which is typically stored in the BucketMapper itself)
  // for this QuotaLimitHeuristic.
  class BucketMapper {
   public:
    virtual ~BucketMapper() {}
    // In most cases, this should simply extract item IDs from the arguments
    // (e.g for bookmark operations involving an existing item). If a problem
    // occurs while parsing |args|, the function aborts - buckets may be non-
    // empty). The expectation is that invalid args and associated errors are
    // handled by the ExtensionFunction itself so we don't concern ourselves.
    // |args| must be a list.
    virtual void GetBucketsForArgs(const base::Value::List& args,
                                   BucketList* buckets) = 0;
  };

  // Maps all calls to the same bucket, regardless of |args|, for this
  // QuotaLimitHeuristic.
  class SingletonBucketMapper : public BucketMapper {
   public:
    SingletonBucketMapper() {}

    SingletonBucketMapper(const SingletonBucketMapper&) = delete;
    SingletonBucketMapper& operator=(const SingletonBucketMapper&) = delete;

    ~SingletonBucketMapper() override = default;
    void GetBucketsForArgs(const base::Value::List& args,
                           BucketList* buckets) override;

   private:
    Bucket bucket_;
  };

  QuotaLimitHeuristic(const Config& config,
                      std::unique_ptr<BucketMapper> map,
                      const std::string& name);

  QuotaLimitHeuristic(const QuotaLimitHeuristic&) = delete;
  QuotaLimitHeuristic& operator=(const QuotaLimitHeuristic&) = delete;

  virtual ~QuotaLimitHeuristic();

  // Determines if sufficient quota exists (according to the Apply
  // implementation of a derived class) to perform an operation with |args|,
  // based on the history of similar operations with similar arguments (which
  // is retrieved using the BucketMapper).
  // |args| must be a list.
  bool ApplyToArgs(const base::Value::List& args,
                   const base::TimeTicks& event_time);

  // Returns an error formatted according to this heuristic.
  std::string GetError() const;

 protected:
  const Config& config() { return config_; }

  // Determine if the new event occurring at |event_time| involving |bucket|
  // constitutes a quota violation according to this heuristic.
  virtual bool Apply(Bucket* bucket, const base::TimeTicks& event_time) = 0;

 private:
  friend class QuotaLimitHeuristicTest;

  const Config config_;

  // The mapper used in Map. Cannot be null.
  std::unique_ptr<BucketMapper> bucket_mapper_;

  // The name of the heuristic for formatting error messages.
  std::string name_;
};

// A simple per-item heuristic to limit the number of events that can occur in
// a given period of time; e.g "no more than 100 events in an hour".
class QuotaService::TimedLimit : public QuotaLimitHeuristic {
 public:
  TimedLimit(const Config& config,
             std::unique_ptr<BucketMapper> map,
             const std::string& name)
      : QuotaLimitHeuristic(config, std::move(map), name) {}
  bool Apply(Bucket* bucket, const base::TimeTicks& event_time) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_QUOTA_SERVICE_H_
