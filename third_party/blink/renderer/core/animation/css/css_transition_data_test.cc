// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_transition_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(CSSTransitionData, TransitionsMatchForStyleRecalc_Initial) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<CSSTransitionData> transition1 =
      std::make_unique<CSSTransitionData>();
  std::unique_ptr<CSSTransitionData> transition2 =
      std::make_unique<CSSTransitionData>();
  EXPECT_TRUE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

TEST(CSSTransitionData, TransitionsMatchForStyleRecalc_CubicBezierSameObject) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<CSSTransitionData> transition1 =
      std::make_unique<CSSTransitionData>();
  std::unique_ptr<CSSTransitionData> transition2 =
      std::make_unique<CSSTransitionData>();
  scoped_refptr<TimingFunction> func =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  transition1->TimingFunctionList().push_back(func);
  transition2->TimingFunctionList().push_back(func);
  EXPECT_TRUE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

TEST(CSSTransitionData,
     TransitionsMatchForStyleRecalc_CubicBezierDifferentObjects) {
  std::unique_ptr<CSSTransitionData> transition1 =
      std::make_unique<CSSTransitionData>();
  std::unique_ptr<CSSTransitionData> transition2 =
      std::make_unique<CSSTransitionData>();
  scoped_refptr<TimingFunction> func1 =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  scoped_refptr<TimingFunction> func2 =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  transition1->TimingFunctionList().push_back(func1);
  transition2->TimingFunctionList().push_back(func2);
  EXPECT_TRUE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

TEST(CSSTransitionData,
     TransitionsMatchForStyleRecalc_CubicBezierDifferentValues) {
  std::unique_ptr<CSSTransitionData> transition1 =
      std::make_unique<CSSTransitionData>();
  std::unique_ptr<CSSTransitionData> transition2 =
      std::make_unique<CSSTransitionData>();
  scoped_refptr<TimingFunction> func1 =
      CubicBezierTimingFunction::Create(0.1f, 0.25f, 0.9f, 0.57f);
  scoped_refptr<TimingFunction> func2 =
      CubicBezierTimingFunction::Create(0.2f, 0.2f, 0.9f, 0.7f);
  transition1->TimingFunctionList().push_back(func1);
  transition2->TimingFunctionList().push_back(func2);
  EXPECT_FALSE(transition1->TransitionsMatchForStyleRecalc(*transition2));
}

}  // namespace blink
