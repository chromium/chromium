// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_CORE_TEST_BASE_H_
#define MOJO_CORE_CORE_TEST_BASE_H_

#include <stddef.h>

#include "base/synchronization/lock.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {

class Core;

namespace test {

class CoreTestBase_MockHandleInfo;

class CoreTestBase : public testing::Test {
 public:
  using MockHandleInfo = CoreTestBase_MockHandleInfo;

  CoreTestBase();

  CoreTestBase(const CoreTestBase&) = delete;
  CoreTestBase& operator=(const CoreTestBase&) = delete;

  ~CoreTestBase() override;

 protected:
  // |info| must remain alive until the returned handle is closed.
  MojoHandle CreateMockHandle(MockHandleInfo* info);

  Core* core();
};

class CoreTestBase_MockHandleInfo {
 public:
  CoreTestBase_MockHandleInfo();

  CoreTestBase_MockHandleInfo(const CoreTestBase_MockHandleInfo&) = delete;
  CoreTestBase_MockHandleInfo& operator=(const CoreTestBase_MockHandleInfo&) =
      delete;

  ~CoreTestBase_MockHandleInfo();

  unsigned GetCtorCallCount() const;
  unsigned GetDtorCallCount() const;
  unsigned GetCloseCallCount() const;
  unsigned GetWriteMessageCallCount() const;
  unsigned GetReadMessageCallCount() const;
  unsigned GetWriteDataCallCount() const;
  unsigned GetBeginWriteDataCallCount() const;
  unsigned GetEndWriteDataCallCount() const;
  unsigned GetReadDataCallCount() const;
  unsigned GetBeginReadDataCallCount() const;
  unsigned GetEndReadDataCallCount() const;

  // For use by |MockDispatcher|:
  void IncrementCtorCallCount();
  void IncrementDtorCallCount();
  void IncrementCloseCallCount();
  void IncrementWriteMessageCallCount();
  void IncrementReadMessageCallCount();
  void IncrementWriteDataCallCount();
  void IncrementBeginWriteDataCallCount();
  void IncrementEndWriteDataCallCount();
  void IncrementReadDataCallCount();
  void IncrementBeginReadDataCallCount();
  void IncrementEndReadDataCallCount();

 private:
  mutable base::Lock lock_;  // Protects the following members.
  unsigned ctor_call_count_;
  unsigned dtor_call_count_;
  unsigned close_call_count_;
  unsigned write_message_call_count_;
  unsigned read_message_call_count_;
  unsigned write_data_call_count_;
  unsigned begin_write_data_call_count_;
  unsigned end_write_data_call_count_;
  unsigned read_data_call_count_;
  unsigned begin_read_data_call_count_;
  unsigned end_read_data_call_count_;
};

}  // namespace test
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_CORE_TEST_BASE_H_
