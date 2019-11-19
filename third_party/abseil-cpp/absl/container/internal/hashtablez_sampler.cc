// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/container/internal/hashtablez_sampler.h"

#include <atomic>
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>

#include "absl/base/attributes.h"
#include "absl/container/internal/have_sse.h"
#include "absl/debugging/stacktrace.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

namespace absl {
namespace container_internal {
constexpr int HashtablezInfo::kMaxStackDepth;

namespace {
ABSL_CONST_INIT std::atomic<bool> g_hashtablez_enabled{
    false
};
ABSL_CONST_INIT std::atomic<int32_t> g_hashtablez_sample_parameter{1 << 10};
ABSL_CONST_INIT std::atomic<int32_t> g_hashtablez_max_samples{1 << 20};

// Returns the next pseudo-random value.
// pRNG is: aX+b mod c with a = 0x5DEECE66D, b =  0xB, c = 1<<48
// This is the lrand64 generator.
uint64_t NextRandom(uint64_t rnd) {
  const uint64_t prng_mult = uint64_t{0x5DEECE66D};
  const uint64_t prng_add = 0xB;
  const uint64_t prng_mod_power = 48;
  const uint64_t prng_mod_mask = ~(~uint64_t{0} << prng_mod_power);
  return (prng_mult * rnd + prng_add) & prng_mod_mask;
}

// Generates a geometric variable with the specified mean.
// This is done by generating a random number between 0 and 1 and applying
// the inverse cumulative distribution function for an exponential.
// Specifically: Let m be the inverse of the sample period, then
// the probability distribution function is m*exp(-mx) so the CDF is
// p = 1 - exp(-mx), so
// q = 1 - p = exp(-mx)
// log_e(q) = -mx
// -log_e(q)/m = x
// log_2(q) * (-log_e(2) * 1/m) = x
// In the code, q is actually in the range 1 to 2**26, hence the -26 below
//
int64_t GetGeometricVariable(int64_t mean) {
#if ABSL_HAVE_THREAD_LOCAL
  thread_local
#else   // ABSL_HAVE_THREAD_LOCAL
  // SampleSlow and hence GetGeometricVariable is guarded by a single mutex when
  // there are not thread locals.  Thus, a single global rng is acceptable for
  // that case.
  static
#endif  // ABSL_HAVE_THREAD_LOCAL
      uint64_t rng = []() {
        // We don't get well distributed numbers from this so we call
        // NextRandom() a bunch to mush the bits around.  We use a global_rand
        // to handle the case where the same thread (by memory address) gets
        // created and destroyed repeatedly.
        ABSL_CONST_INIT static std::atomic<uint32_t> global_rand(0);
        uint64_t r = reinterpret_cast<uint64_t>(&rng) +
                   global_rand.fetch_add(1, std::memory_order_relaxed);
        for (int i = 0; i < 20; ++i) {
          r = NextRandom(r);
        }
        return r;
      }();

  rng = NextRandom(rng);

  // Take the top 26 bits as the random number
  // (This plus the 1<<58 sampling bound give a max possible step of
  // 5194297183973780480 bytes.)
  const uint64_t prng_mod_power = 48;  // Number of bits in prng
  // The uint32_t cast is to prevent a (hard-to-reproduce) NAN
  // under piii debug for some binaries.
  double q = static_cast<uint32_t>(rng >> (prng_mod_power - 26)) + 1.0;
  // Put the computed p-value through the CDF of a geometric.
  double interval = (log2(q) - 26) * (-std::log(2.0) * mean);

  // Very large values of interval overflow int64_t. If we happen to
  // hit such improbable condition, we simply cheat and clamp interval
  // to largest supported value.
  if (interval > static_cast<double>(std::numeric_limits<int64_t>::max() / 2)) {
    return std::numeric_limits<int64_t>::max() / 2;
  }

  // Small values of interval are equivalent to just sampling next time.
  if (interval < 1) {
    return 1;
  }
  return static_cast<int64_t>(interval);
}

}  // namespace

HashtablezSampler& HashtablezSampler::Global() {
  static auto* sampler = new HashtablezSampler();
  return *sampler;
}

HashtablezSampler::DisposeCallback HashtablezSampler::SetDisposeCallback(
    DisposeCallback f) {
  return dispose_.exchange(f, std::memory_order_relaxed);
}

HashtablezInfo::HashtablezInfo() { PrepareForSampling(); }
HashtablezInfo::~HashtablezInfo() = default;

void HashtablezInfo::PrepareForSampling() {
  capacity.store(0, std::memory_order_relaxed);
  size.store(0, std::memory_order_relaxed);
  num_erases.store(0, std::memory_order_relaxed);
  max_probe_length.store(0, std::memory_order_relaxed);
  total_probe_length.store(0, std::memory_order_relaxed);
  hashes_bitwise_or.store(0, std::memory_order_relaxed);
  hashes_bitwise_and.store(~size_t{}, std::memory_order_relaxed);

  create_time = absl::Now();
  // The inliner makes hardcoded skip_count difficult (especially when combined
  // with LTO).  We use the ability to exclude stacks by regex when encoding
  // instead.
  depth = absl::GetStackTrace(stack, HashtablezInfo::kMaxStackDepth,
                              /* skip_count= */ 0);
  dead = nullptr;
}

HashtablezSampler::HashtablezSampler()
    : dropped_samples_(0), size_estimate_(0), all_(nullptr), dispose_(nullptr) {
  absl::MutexLock l(&graveyard_.init_mu);
  graveyard_.dead = &graveyard_;
}

HashtablezSampler::~HashtablezSampler() {
  HashtablezInfo* s = all_.load(std::memory_order_acquire);
  while (s != nullptr) {
    HashtablezInfo* next = s->next;
    delete s;
    s = next;
  }
}

void HashtablezSampler::PushNew(HashtablezInfo* sample) {
  sample->next = all_.load(std::memory_order_relaxed);
  while (!all_.compare_exchange_weak(sample->next, sample,
                                     std::memory_order_release,
                                     std::memory_order_relaxed)) {
  }
}

void HashtablezSampler::PushDead(HashtablezInfo* sample) {
  if (auto* dispose = dispose_.load(std::memory_order_relaxed)) {
    dispose(*sample);
  }

  absl::MutexLock graveyard_lock(&graveyard_.init_mu);
  absl::MutexLock sample_lock(&sample->init_mu);
  sample->dead = graveyard_.dead;
  graveyard_.dead = sample;
}

HashtablezInfo* HashtablezSampler::PopDead() {
  absl::MutexLock graveyard_lock(&graveyard_.init_mu);

  // The list is circular, so eventually it collapses down to
  //   graveyard_.dead == &graveyard_
  // when it is empty.
  HashtablezInfo* sample = graveyard_.dead;
  if (sample == &graveyard_) return nullptr;

  absl::MutexLock sample_lock(&sample->init_mu);
  graveyard_.dead = sample->dead;
  sample->PrepareForSampling();
  return sample;
}

HashtablezInfo* HashtablezSampler::Register() {
  int64_t size = size_estimate_.fetch_add(1, std::memory_order_relaxed);
  if (size > g_hashtablez_max_samples.load(std::memory_order_relaxed)) {
    size_estimate_.fetch_sub(1, std::memory_order_relaxed);
    dropped_samples_.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
  }

  HashtablezInfo* sample = PopDead();
  if (sample == nullptr) {
    // Resurrection failed.  Hire a new warlock.
    sample = new HashtablezInfo();
    PushNew(sample);
  }

  return sample;
}

void HashtablezSampler::Unregister(HashtablezInfo* sample) {
  PushDead(sample);
  size_estimate_.fetch_sub(1, std::memory_order_relaxed);
}

int64_t HashtablezSampler::Iterate(
    const std::function<void(const HashtablezInfo& stack)>& f) {
  HashtablezInfo* s = all_.load(std::memory_order_acquire);
  while (s != nullptr) {
    absl::MutexLock l(&s->init_mu);
    if (s->dead == nullptr) {
      f(*s);
    }
    s = s->next;
  }

  return dropped_samples_.load(std::memory_order_relaxed);
}

HashtablezInfo* SampleSlow(int64_t* next_sample) {
  if (kAbslContainerInternalSampleEverything) {
    *next_sample = 1;
    return HashtablezSampler::Global().Register();
  }

  bool first = *next_sample < 0;
  *next_sample = GetGeometricVariable(
      g_hashtablez_sample_parameter.load(std::memory_order_relaxed));

  // g_hashtablez_enabled can be dynamically flipped, we need to set a threshold
  // low enough that we will start sampling in a reasonable time, so we just use
  // the default sampling rate.
  if (!g_hashtablez_enabled.load(std::memory_order_relaxed)) return nullptr;

  // We will only be negative on our first count, so we should just retry in
  // that case.
  if (first) {
    if (ABSL_PREDICT_TRUE(--*next_sample > 0)) return nullptr;
    return SampleSlow(next_sample);
  }

  return HashtablezSampler::Global().Register();
}

#if ABSL_PER_THREAD_TLS == 1
ABSL_PER_THREAD_TLS_KEYWORD int64_t global_next_sample = 0;
#endif  // ABSL_PER_THREAD_TLS == 1

void UnsampleSlow(HashtablezInfo* info) {
  HashtablezSampler::Global().Unregister(info);
}

void RecordInsertSlow(HashtablezInfo* info, size_t hash,
                      size_t distance_from_desired) {
  // SwissTables probe in groups of 16, so scale this to count items probes and
  // not offset from desired.
  size_t probe_length = distance_from_desired;
#if SWISSTABLE_HAVE_SSE2
  probe_length /= 16;
#else
  probe_length /= 8;
#endif

  info->hashes_bitwise_and.fetch_and(hash, std::memory_order_relaxed);
  info->hashes_bitwise_or.fetch_or(hash, std::memory_order_relaxed);
  info->max_probe_length.store(
      std::max(info->max_probe_length.load(std::memory_order_relaxed),
               probe_length),
      std::memory_order_relaxed);
  info->total_probe_length.fetch_add(probe_length, std::memory_order_relaxed);
  info->size.fetch_add(1, std::memory_order_relaxed);
}

void SetHashtablezEnabled(bool enabled) {
  g_hashtablez_enabled.store(enabled, std::memory_order_release);
}

void SetHashtablezSampleParameter(int32_t rate) {
  if (rate > 0) {
    g_hashtablez_sample_parameter.store(rate, std::memory_order_release);
  } else {
    ABSL_RAW_LOG(ERROR, "Invalid hashtablez sample rate: %lld",
                 static_cast<long long>(rate));  // NOLINT(runtime/int)
  }
}

void SetHashtablezMaxSamples(int32_t max) {
  if (max > 0) {
    g_hashtablez_max_samples.store(max, std::memory_order_release);
  } else {
    ABSL_RAW_LOG(ERROR, "Invalid hashtablez max samples: %lld",
                 static_cast<long long>(max));  // NOLINT(runtime/int)
  }
}

}  // namespace container_internal
}  // namespace absl
