// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/threadpool.h"

#include <windows.h>

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

void __stdcall EmptyCallBack(void*, unsigned char) {}

void __stdcall TestCallBack(void* context, unsigned char) {
  HANDLE event = reinterpret_cast<HANDLE>(context);
  ::SetEvent(event);
}

namespace sandbox {

// Test that register and unregister work, part 1.
TEST(IPCTest, ThreadPoolRegisterTest1) {
  ThreadPool thread_pool;

  EXPECT_EQ(0u, thread_pool.OutstandingWaits());

  HANDLE event1 = ::CreateEventW(nullptr, false, false, nullptr);
  HANDLE event2 = ::CreateEventW(nullptr, false, false, nullptr);

  uint32_t context = 0;
  EXPECT_FALSE(thread_pool.RegisterWait(0, event1, EmptyCallBack, &context));
  EXPECT_EQ(0u, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.RegisterWait(this, event1, EmptyCallBack, &context));
  EXPECT_EQ(1u, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.RegisterWait(this, event2, EmptyCallBack, &context));
  EXPECT_EQ(2u, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(this));
  EXPECT_EQ(0u, thread_pool.OutstandingWaits());

  EXPECT_TRUE(::CloseHandle(event1));
  EXPECT_TRUE(::CloseHandle(event2));
}

// Test that register and unregister work, part 2.
TEST(IPCTest, ThreadPoolRegisterTest2) {
  ThreadPool thread_pool;

  HANDLE event1 = ::CreateEventW(nullptr, false, false, nullptr);
  HANDLE event2 = ::CreateEventW(nullptr, false, false, nullptr);

  uint32_t context = 0;
  uint32_t c1 = 0;
  uint32_t c2 = 0;

  EXPECT_TRUE(thread_pool.RegisterWait(&c1, event1, EmptyCallBack, &context));
  EXPECT_EQ(1u, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.RegisterWait(&c2, event2, EmptyCallBack, &context));
  EXPECT_EQ(2u, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c2));
  EXPECT_EQ(1u, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c2));
  EXPECT_EQ(1u, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c1));
  EXPECT_EQ(0u, thread_pool.OutstandingWaits());

  EXPECT_TRUE(::CloseHandle(event1));
  EXPECT_TRUE(::CloseHandle(event2));
}

// Test that the thread pool has at least a thread that services an event.
// Test that when the event is un-registered is no longer serviced.
TEST(IPCTest, ThreadPoolSignalAndWaitTest) {
  ThreadPool thread_pool;

  // The events are auto reset and start not signaled.
  HANDLE event1 = ::CreateEventW(nullptr, false, false, nullptr);
  HANDLE event2 = ::CreateEventW(nullptr, false, false, nullptr);

  EXPECT_TRUE(thread_pool.RegisterWait(this, event1, TestCallBack, event2));

  EXPECT_EQ(WAIT_OBJECT_0, ::SignalObjectAndWait(event1, event2, 5000, false));
  EXPECT_EQ(WAIT_OBJECT_0, ::SignalObjectAndWait(event1, event2, 5000, false));

  EXPECT_TRUE(thread_pool.UnRegisterWaits(this));
  EXPECT_EQ(0u, thread_pool.OutstandingWaits());

  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            ::SignalObjectAndWait(event1, event2, 1000, false));

  EXPECT_TRUE(::CloseHandle(event1));
  EXPECT_TRUE(::CloseHandle(event2));
}

}  // namespace sandbox
