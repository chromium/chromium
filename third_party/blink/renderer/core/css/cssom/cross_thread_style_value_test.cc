// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"

#include <memory>
#include <utility>

#include "base/memory/values_equivalent.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_color_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_color.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class CrossThreadStyleValueTest : public testing::Test {
 public:
  void ShutDown(base::WaitableEvent* waitable_event) {
    DCHECK(!IsMainThread());
    waitable_event->Signal();
  }

  void ShutDownThread() {
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&CrossThreadStyleValueTest::ShutDown,
                            CrossThreadUnretained(this),
                            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
  }

  void CheckUnsupportedValue(
      base::WaitableEvent* waitable_event,
      std::unique_ptr<CrossThreadUnsupportedValue> value) {
    DCHECK(!IsMainThread());

    EXPECT_EQ(value->value_, "Unsupported");
    waitable_event->Signal();
  }

  void CheckKeywordValue(base::WaitableEvent* waitable_event,
                         std::unique_ptr<CrossThreadKeywordValue> value) {
    DCHECK(!IsMainThread());

    EXPECT_EQ(value->keyword_value_, "Keyword");
    waitable_event->Signal();
  }

  void CheckUnparsedValue(base::WaitableEvent* waitable_event,
                          std::unique_ptr<CrossThreadUnparsedValue> value) {
    DCHECK(!IsMainThread());

    EXPECT_EQ(value->value_, "Unparsed");
    waitable_event->Signal();
  }

  void CheckUnitValue(base::WaitableEvent* waitable_event,
                      std::unique_ptr<CrossThreadUnitValue> value) {
    DCHECK(!IsMainThread());

    EXPECT_EQ(value->value_, 1);
    EXPECT_EQ(value->unit_, CSSPrimitiveValue::UnitType::kDegrees);
    waitable_event->Signal();
  }

  void CheckColorValue(base::WaitableEvent* waitable_event,
                       std::unique_ptr<CrossThreadColorValue> value) {
    DCHECK(!IsMainThread());

    EXPECT_EQ(value->value_, Color(0, 255, 0));
    waitable_event->Signal();
  }

 protected:
  std::unique_ptr<blink::NonMainThread> thread_;
};

// Ensure that a CrossThreadUnsupportedValue can be safely passed cross
// threads.
TEST_F(CrossThreadStyleValueTest, PassUnsupportedValueCrossThread) {
  std::unique_ptr<CrossThreadUnsupportedValue> value =
      std::make_unique<CrossThreadUnsupportedValue>("Unsupported");
  DCHECK(value);

  // Use a Thread to emulate worklet thread.
  thread_ = blink::NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread).SetSupportsGC(true));
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&CrossThreadStyleValueTest::CheckUnsupportedValue,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event),
                          std::move(value)));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadUnsupportedValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadUnsupportedValue> value =
      std::make_unique<CrossThreadUnsupportedValue>("Unsupported");
  DCHECK(value);

  const CSSStyleValue* const style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kUnknownType);
  EXPECT_EQ(style_value->CSSText(), "Unsupported");
}

TEST_F(CrossThreadStyleValueTest, PassUnparsedValueCrossThread) {
  std::unique_ptr<CrossThreadUnparsedValue> value =
      std::make_unique<CrossThreadUnparsedValue>("Unparsed");
  DCHECK(value);

  // Use a Thread to emulate worklet thread.
  thread_ = blink::NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread).SetSupportsGC(true));
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&CrossThreadStyleValueTest::CheckUnparsedValue,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event),
                          std::move(value)));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadUnparsedValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadUnparsedValue> value =
      std::make_unique<CrossThreadUnparsedValue>("Unparsed");
  DCHECK(value);

  CSSStyleValue* style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kUnparsedType);
  EXPECT_EQ(static_cast<CSSUnparsedValue*>(style_value)->ToUnparsedString(),
            "Unparsed");
}

TEST_F(CrossThreadStyleValueTest, PassKeywordValueCrossThread) {
  std::unique_ptr<CrossThreadKeywordValue> value =
      std::make_unique<CrossThreadKeywordValue>("Keyword");
  DCHECK(value);

  // Use a Thread to emulate worklet thread.
  thread_ = blink::NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread).SetSupportsGC(true));
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&CrossThreadStyleValueTest::CheckKeywordValue,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event),
                          std::move(value)));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadKeywordValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadKeywordValue> value =
      std::make_unique<CrossThreadKeywordValue>("Keyword");
  DCHECK(value);

  CSSStyleValue* style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kKeywordType);
  EXPECT_EQ(static_cast<CSSKeywordValue*>(style_value)->value(), "Keyword");
}

TEST_F(CrossThreadStyleValueTest, PassUnitValueCrossThread) {
  std::unique_ptr<CrossThreadUnitValue> value =
      std::make_unique<CrossThreadUnitValue>(
          1, CSSPrimitiveValue::UnitType::kDegrees);
  DCHECK(value);

  // Use a Thread to emulate worklet thread.
  thread_ = blink::NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread).SetSupportsGC(true));
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&CrossThreadStyleValueTest::CheckUnitValue,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event),
                          std::move(value)));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadUnitValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadUnitValue> value =
      std::make_unique<CrossThreadUnitValue>(
          1, CSSPrimitiveValue::UnitType::kDegrees);
  DCHECK(value);

  CSSStyleValue* style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(), CSSStyleValue::StyleValueType::kUnitType);
  EXPECT_EQ(static_cast<CSSUnitValue*>(style_value)->value(), 1);
  EXPECT_EQ(static_cast<CSSUnitValue*>(style_value)->unit(), "deg");
}

TEST_F(CrossThreadStyleValueTest, PassColorValueCrossThread) {
  std::unique_ptr<CrossThreadColorValue> value =
      std::make_unique<CrossThreadColorValue>(Color(0, 255, 0));
  DCHECK(value);

  // Use a Thread to emulate worklet thread.
  thread_ = blink::NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread).SetSupportsGC(true));
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&CrossThreadStyleValueTest::CheckColorValue,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event),
                          std::move(value)));
  waitable_event.Wait();

  ShutDownThread();
}

TEST_F(CrossThreadStyleValueTest, CrossThreadColorValueToCSSStyleValue) {
  std::unique_ptr<CrossThreadColorValue> value =
      std::make_unique<CrossThreadColorValue>(Color(0, 255, 0));
  DCHECK(value);

  CSSStyleValue* style_value = value->ToCSSStyleValue();
  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kUnsupportedColorType);
  EXPECT_EQ(static_cast<CSSUnsupportedColor*>(style_value)->Value(),
            Color(0, 255, 0));
}

TEST_F(CrossThreadStyleValueTest, ComparingNullValues) {
  // Two null values are equal to each other.
  std::unique_ptr<CrossThreadStyleValue> null_value1(nullptr);
  std::unique_ptr<CrossThreadStyleValue> null_value2(nullptr);
  EXPECT_TRUE(base::ValuesEquivalent(null_value1, null_value2));

  // If one argument is null and the other isn't they are never equal.
  std::unique_ptr<CrossThreadStyleValue> keyword_value(
      new CrossThreadKeywordValue("keyword"));
  std::unique_ptr<CrossThreadStyleValue> unit_value(
      new CrossThreadUnitValue(1, CSSPrimitiveValue::UnitType::kDegrees));
  std::unique_ptr<CrossThreadStyleValue> unsupported_value(
      new CrossThreadUnsupportedValue("unsupported"));

  EXPECT_FALSE(base::ValuesEquivalent(null_value1, keyword_value));
  EXPECT_FALSE(base::ValuesEquivalent(null_value1, unit_value));
  EXPECT_FALSE(base::ValuesEquivalent(null_value1, unsupported_value));
  EXPECT_FALSE(base::ValuesEquivalent(keyword_value, null_value1));
  EXPECT_FALSE(base::ValuesEquivalent(unit_value, null_value1));
  EXPECT_FALSE(base::ValuesEquivalent(unsupported_value, null_value1));
}

TEST_F(CrossThreadStyleValueTest, ComparingDifferentTypes) {
  // Mismatching types are never equal.
  std::unique_ptr<CrossThreadStyleValue> keyword_value(
      new CrossThreadKeywordValue("keyword"));
  std::unique_ptr<CrossThreadStyleValue> unit_value(
      new CrossThreadUnitValue(1, CSSPrimitiveValue::UnitType::kDegrees));
  std::unique_ptr<CrossThreadStyleValue> unsupported_value(
      new CrossThreadUnsupportedValue("unsupported"));

  EXPECT_FALSE(base::ValuesEquivalent(keyword_value, unit_value));
  EXPECT_FALSE(base::ValuesEquivalent(keyword_value, unsupported_value));
  EXPECT_FALSE(base::ValuesEquivalent(unit_value, unsupported_value));
  EXPECT_FALSE(base::ValuesEquivalent(unit_value, keyword_value));
  EXPECT_FALSE(base::ValuesEquivalent(unsupported_value, keyword_value));
  EXPECT_FALSE(base::ValuesEquivalent(unsupported_value, unit_value));
}

TEST_F(CrossThreadStyleValueTest, ComparingCrossThreadKeywordValue) {
  // CrossThreadKeywordValues are compared on their keyword; if it is equal then
  // so are they.
  std::unique_ptr<CrossThreadStyleValue> keyword_value_1(
      new CrossThreadKeywordValue("keyword"));
  std::unique_ptr<CrossThreadStyleValue> keyword_value_2(
      new CrossThreadKeywordValue("keyword"));
  std::unique_ptr<CrossThreadStyleValue> keyword_value_3(
      new CrossThreadKeywordValue("different"));

  EXPECT_TRUE(base::ValuesEquivalent(keyword_value_1, keyword_value_2));
  EXPECT_FALSE(base::ValuesEquivalent(keyword_value_1, keyword_value_3));
}

TEST_F(CrossThreadStyleValueTest, ComparingCrossThreadUnitValue) {
  // CrossThreadUnitValues are compared based on their value and unit type; both
  // have to match. There are a lot of unit types; we just test a single sample.
  std::unique_ptr<CrossThreadStyleValue> unit_value_1(
      new CrossThreadUnitValue(1, CSSPrimitiveValue::UnitType::kDegrees));

  // Same value, same unit.
  std::unique_ptr<CrossThreadStyleValue> unit_value_2(
      new CrossThreadUnitValue(1, CSSPrimitiveValue::UnitType::kDegrees));
  EXPECT_TRUE(base::ValuesEquivalent(unit_value_1, unit_value_2));

  // Same value, different unit.
  std::unique_ptr<CrossThreadStyleValue> unit_value_3(
      new CrossThreadUnitValue(1, CSSPrimitiveValue::UnitType::kPoints));
  EXPECT_FALSE(base::ValuesEquivalent(unit_value_1, unit_value_3));

  // Different value, same unit.
  std::unique_ptr<CrossThreadStyleValue> unit_value_4(
      new CrossThreadUnitValue(2, CSSPrimitiveValue::UnitType::kDegrees));
  EXPECT_FALSE(base::ValuesEquivalent(unit_value_1, unit_value_4));
}

TEST_F(CrossThreadStyleValueTest, ComparingCrossThreadColorValue) {
  // CrossThreadColorValues are compared on their color channel values; all
  // channels must match.
  std::unique_ptr<CrossThreadStyleValue> color_value_1(
      new CrossThreadColorValue(Color(0, 0, 0)));
  std::unique_ptr<CrossThreadStyleValue> color_value_2(
      new CrossThreadColorValue(Color(0, 0, 0)));
  std::unique_ptr<CrossThreadStyleValue> color_value_3(
      new CrossThreadColorValue(Color(0, 255, 0)));

  EXPECT_TRUE(base::ValuesEquivalent(color_value_1, color_value_2));
  EXPECT_FALSE(base::ValuesEquivalent(color_value_1, color_value_3));
}

TEST_F(CrossThreadStyleValueTest, ComparingCrossThreadUnsupportedValue) {
  // CrossThreadUnsupportedValues are compared on their value; if it is equal
  // then so are they.
  std::unique_ptr<CrossThreadStyleValue> unsupported_value_1(
      new CrossThreadUnsupportedValue("value"));
  std::unique_ptr<CrossThreadStyleValue> unsupported_value_2(
      new CrossThreadUnsupportedValue("value"));
  std::unique_ptr<CrossThreadStyleValue> unsupported_value_3(
      new CrossThreadUnsupportedValue("different"));

  EXPECT_TRUE(base::ValuesEquivalent(unsupported_value_1, unsupported_value_2));
  EXPECT_FALSE(
      base::ValuesEquivalent(unsupported_value_1, unsupported_value_3));
}

}  // namespace blink
