// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We want to measure the similarity of two sequences of bytes as a surrogate
// for measuring how well a second sequence will compress differentially to the
// first sequence.

#include "courgette/difference_estimator.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <unordered_set>

namespace courgette {

// Our difference measure is the number of k-tuples that occur in Subject that
// don't occur in Base.
const int kTupleSize = 4;

namespace {

static_assert(kTupleSize >= 4 && kTupleSize <= 8,
              "kTupleSize should be between 4 and 8");

size_t HashTuple(const uint8_t* source) {
  size_t hash1 = *reinterpret_cast<const uint32_t*>(source);
  size_t hash2 = *reinterpret_cast<const uint32_t*>(source + kTupleSize - 4);
  size_t hash = ((hash1 * 17 + hash2 * 37) + (hash1 >> 17)) ^ (hash2 >> 23);
  return hash;
}

bool RegionsEqual(const Region& a, const Region& b) {
  if (a.length() != b.length())
    return false;
  return memcmp(a.start(), b.start(), a.length()) == 0;
}

}  // anonymous namepace

class DifferenceEstimator::Base {
 public:
  explicit Base(const Region& region) : region_(region) { }

  Base(const Base&) = delete;
  Base& operator=(const Base&) = delete;

  void Init() {
    if (region_.length() < kTupleSize)
      return;
    const uint8_t* start = region_.start();
    const uint8_t* end = region_.end() - (kTupleSize - 1);
    for (const uint8_t* p = start; p < end; ++p) {
      size_t hash = HashTuple(p);
      hashes_.insert(hash);
    }
  }

  const Region& region() const { return region_; }

 private:
  Region region_;
  std::unordered_set<size_t> hashes_;

  friend class DifferenceEstimator;
};

class DifferenceEstimator::Subject {
 public:
  explicit Subject(const Region& region) : region_(region) {}

  Subject(const Subject&) = delete;
  Subject& operator=(const Subject&) = delete;

  const Region& region() const { return region_; }

 private:
  Region region_;
};

DifferenceEstimator::DifferenceEstimator() = default;

DifferenceEstimator::~DifferenceEstimator() {
  for (size_t i = 0;  i < owned_bases_.size();  ++i)
    delete owned_bases_[i];
  for (size_t i = 0;  i < owned_subjects_.size();  ++i)
    delete owned_subjects_[i];
}

DifferenceEstimator::Base* DifferenceEstimator::MakeBase(const Region& region) {
  Base* base = new Base(region);
  base->Init();
  owned_bases_.push_back(base);
  return base;
}

DifferenceEstimator::Subject* DifferenceEstimator::MakeSubject(
    const Region& region) {
  Subject* subject = new Subject(region);
  owned_subjects_.push_back(subject);
  return subject;
}

size_t DifferenceEstimator::Measure(Base* base, Subject* subject) {
  size_t mismatches = 0;
  if (subject->region().length() >= kTupleSize) {
    const uint8_t* start = subject->region().start();
    const uint8_t* end = subject->region().end() - (kTupleSize - 1);

    const uint8_t* p = start;
    while (p < end) {
      size_t hash = HashTuple(p);
      if (base->hashes_.find(hash) == base->hashes_.end()) {
        ++mismatches;
      }
      p += 1;
    }
  }

  if (mismatches == 0) {
    if (RegionsEqual(base->region(), subject->region()))
      return 0;
  }
  ++mismatches;  // Guarantee not zero.
  return mismatches;
}

}  // namespace
