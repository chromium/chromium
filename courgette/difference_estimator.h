// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A DifferenceEstimator class provides a means for quickly estimating the
// difference between two regions of memory.

#ifndef COURGETTE_DIFFERENCE_ESTIMATOR_H_
#define COURGETTE_DIFFERENCE_ESTIMATOR_H_

#include <stddef.h>

#include <vector>

#include "courgette/region.h"

namespace courgette {

// A DifferenceEstimator simplifies the task of determining which 'Subject' byte
// strings (stored in regions of memory) are good matches to existing 'Base'
// regions.  The ultimate measure would be to try full differential compression
// and measure the output size, but an estimate that correlates well with the
// full compression is more efficient.
//
// The measure is asymmetric, if the Subject is a small substring of the Base
// then it should match very well.
//
// The comparison is staged: first make Base and Subject objects for the regions
// and then call 'Measure' to get the estimate.  The staging allows multiple
// comparisons to be more efficient by precomputing information used in the
// comparison.
//
class DifferenceEstimator {
 public:
  DifferenceEstimator();

  DifferenceEstimator(const DifferenceEstimator&) = delete;
  DifferenceEstimator& operator=(const DifferenceEstimator&) = delete;

  ~DifferenceEstimator();

  class Base;
  class Subject;

  // This DifferenceEstimator owns the objects returned by MakeBase and
  // MakeSubject.  Caller continues to own memory at |region| and must not free
  // it until ~DifferenceEstimator has been called.
  Base* MakeBase(const Region& region);
  Subject* MakeSubject(const Region& region);

  // Returns a value correlated with the size of the bsdiff or xdelta difference
  // from |base| to |subject|.  Returns zero iff the base and subject regions
  // are bytewise identical.
  size_t Measure(Base* base,  Subject* subject);

 private:
  std::vector<Base*> owned_bases_;
  std::vector<Subject*> owned_subjects_;
};

}  // namespace

#endif  // COURGETTE_DIFFERENCE_ESTIMATOR_H_
