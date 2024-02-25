// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/track_recorder.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
using ::testing::MockFunction;

void CallMockFunction(MockFunction<void()>* function) {
  function->Call();
}

TEST(TrackRecorderTest, CallsOutOnSourceStateEnded) {
  test::TaskEnvironment task_environment;
  MockFunction<void()> callback;
  EXPECT_CALL(callback, Call);

  TrackRecorder<WebMediaStreamSink> recorder(
      WTF::BindOnce(&CallMockFunction, WTF::Unretained(&callback)));
  recorder.OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
}

TEST(TrackRecorderTest, DoesNotCallOutOnAnythingButStateEnded) {
  test::TaskEnvironment task_environment;
  MockFunction<void()> callback;
  EXPECT_CALL(callback, Call).Times(0);

  TrackRecorder<WebMediaStreamSink> recorder(
      WTF::BindOnce(&CallMockFunction, WTF::Unretained(&callback)));
  recorder.OnReadyStateChanged(WebMediaStreamSource::kReadyStateLive);
  recorder.OnReadyStateChanged(WebMediaStreamSource::kReadyStateMuted);
}
}  // namespace
}  // namespace blink
