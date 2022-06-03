// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/track_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
using ::testing::MockFunction;

void CallMockFunction(MockFunction<void()>* function) {
  function->Call();
}

TEST(TrackRecorderTest, CallsOutOnSourceStateEnded) {
  MockFunction<void()> callback;
  EXPECT_CALL(callback, Call);

  TrackRecorder<WebMediaStreamSink> recorder(
      base::BindOnce(&CallMockFunction, base::Unretained(&callback)));
  recorder.OnReadyStateChanged(WebMediaStreamSource::kReadyStateEnded);
}

TEST(TrackRecorderTest, DoesNotCallOutOnAnythingButStateEnded) {
  MockFunction<void()> callback;
  EXPECT_CALL(callback, Call).Times(0);

  TrackRecorder<WebMediaStreamSink> recorder(
      base::BindOnce(&CallMockFunction, base::Unretained(&callback)));
  recorder.OnReadyStateChanged(WebMediaStreamSource::kReadyStateLive);
  recorder.OnReadyStateChanged(WebMediaStreamSource::kReadyStateMuted);
}
}  // namespace
}  // namespace blink
