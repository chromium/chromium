// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_PAINT_AGGREGATOR_H_
#define PPAPI_TESTS_TEST_PAINT_AGGREGATOR_H_

#include "ppapi/tests/test_case.h"

class TestPaintAggregator : public TestCase {
 public:
  TestPaintAggregator(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestInitialState();
  std::string TestSingleInvalidation();
  std::string TestDoubleDisjointInvalidation();
  std::string TestSingleScroll();
  std::string TestDoubleOverlappingScroll();
  std::string TestNegatingScroll();
  std::string TestDiagonalScroll();
  std::string TestContainedPaintAfterScroll();
  std::string TestContainedPaintBeforeScroll();
  std::string TestContainedPaintsBeforeAndAfterScroll();
  std::string TestLargeContainedPaintAfterScroll();
  std::string TestLargeContainedPaintBeforeScroll();
  std::string TestOverlappingPaintBeforeScroll();
  std::string TestOverlappingPaintAfterScroll();
  std::string TestDisjointPaintBeforeScroll();
  std::string TestDisjointPaintAfterScroll();
  std::string TestContainedPaintTrimmedByScroll();
  std::string TestContainedPaintEliminatedByScroll();
  std::string TestContainedPaintAfterScrollTrimmedByScrollDamage();
  std::string TestContainedPaintAfterScrollEliminatedByScrollDamage();
};

#endif  // PPAPI_TESTS_TEST_PAINT_AGGREGATOR_H_
