// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

#if DCHECK_IS_ON()
TEST(ThreadTest, IsBeforeThreadCreated) {
  WTF::SetIsBeforeThreadCreatedForTest();
  EXPECT_TRUE(WTF::IsBeforeThreadCreated());

  ThreadCreationParams params(ThreadType::kTestThread);
  std::unique_ptr<NonMainThread> thread = NonMainThread::CreateThread(params);
  thread.reset();

  EXPECT_FALSE(WTF::IsBeforeThreadCreated());
}
#endif

}  // namespace blink
