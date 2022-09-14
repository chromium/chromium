// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/core_test_base.h"
#include "base/memory/raw_ptr.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "mojo/core/configuration.h"
#include "mojo/core/core.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/user_message_impl.h"

namespace mojo {
namespace core {
namespace test {

namespace {

// MockDispatcher --------------------------------------------------------------

class MockDispatcher : public Dispatcher {
 public:
  static scoped_refptr<MockDispatcher> Create(
      CoreTestBase::MockHandleInfo* info) {
    return base::WrapRefCounted(new MockDispatcher(info));
  }

  MockDispatcher(const MockDispatcher&) = delete;
  MockDispatcher& operator=(const MockDispatcher&) = delete;

  // Dispatcher:
  Type GetType() const override { return Type::UNKNOWN; }

  MojoResult Close() override {
    info_->IncrementCloseCallCount();
    return MOJO_RESULT_OK;
  }

  MojoResult WriteMessage(
      std::unique_ptr<ports::UserMessageEvent> message_event) override {
    info_->IncrementWriteMessageCallCount();
    return MOJO_RESULT_OK;
  }

  MojoResult ReadMessage(
      std::unique_ptr<ports::UserMessageEvent>* message_event) override {
    info_->IncrementReadMessageCallCount();
    return MOJO_RESULT_OK;
  }

  MojoResult WriteData(const void* elements,
                       uint32_t* num_bytes,
                       const MojoWriteDataOptions& options) override {
    info_->IncrementWriteDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult BeginWriteData(void** buffer,
                            uint32_t* buffer_num_bytes,
                            MojoBeginWriteDataFlags flags) override {
    info_->IncrementBeginWriteDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult EndWriteData(uint32_t num_bytes_written) override {
    info_->IncrementEndWriteDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult ReadData(const MojoReadDataOptions& options,
                      void* elements,
                      uint32_t* num_bytes) override {
    info_->IncrementReadDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult BeginReadData(const void** buffer,
                           uint32_t* buffer_num_bytes) override {
    info_->IncrementBeginReadDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

  MojoResult EndReadData(uint32_t num_bytes_read) override {
    info_->IncrementEndReadDataCallCount();
    return MOJO_RESULT_UNIMPLEMENTED;
  }

 private:
  explicit MockDispatcher(CoreTestBase::MockHandleInfo* info) : info_(info) {
    CHECK(info_);
    info_->IncrementCtorCallCount();
  }

  ~MockDispatcher() override { info_->IncrementDtorCallCount(); }

  const raw_ptr<CoreTestBase::MockHandleInfo> info_;
};

}  // namespace

// CoreTestBase ----------------------------------------------------------------

CoreTestBase::CoreTestBase() = default;

CoreTestBase::~CoreTestBase() = default;

MojoHandle CoreTestBase::CreateMockHandle(CoreTestBase::MockHandleInfo* info) {
  scoped_refptr<MockDispatcher> dispatcher = MockDispatcher::Create(info);
  return core()->AddDispatcher(dispatcher);
}

Core* CoreTestBase::core() {
  return Core::Get();
}

// CoreTestBase_MockHandleInfo -------------------------------------------------

CoreTestBase_MockHandleInfo::CoreTestBase_MockHandleInfo()
    : ctor_call_count_(0),
      dtor_call_count_(0),
      close_call_count_(0),
      write_message_call_count_(0),
      read_message_call_count_(0),
      write_data_call_count_(0),
      begin_write_data_call_count_(0),
      end_write_data_call_count_(0),
      read_data_call_count_(0),
      begin_read_data_call_count_(0),
      end_read_data_call_count_(0) {}

CoreTestBase_MockHandleInfo::~CoreTestBase_MockHandleInfo() = default;

unsigned CoreTestBase_MockHandleInfo::GetCtorCallCount() const {
  base::AutoLock locker(lock_);
  return ctor_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetDtorCallCount() const {
  base::AutoLock locker(lock_);
  return dtor_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetCloseCallCount() const {
  base::AutoLock locker(lock_);
  return close_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetWriteMessageCallCount() const {
  base::AutoLock locker(lock_);
  return write_message_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetReadMessageCallCount() const {
  base::AutoLock locker(lock_);
  return read_message_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetWriteDataCallCount() const {
  base::AutoLock locker(lock_);
  return write_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetBeginWriteDataCallCount() const {
  base::AutoLock locker(lock_);
  return begin_write_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetEndWriteDataCallCount() const {
  base::AutoLock locker(lock_);
  return end_write_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetReadDataCallCount() const {
  base::AutoLock locker(lock_);
  return read_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetBeginReadDataCallCount() const {
  base::AutoLock locker(lock_);
  return begin_read_data_call_count_;
}

unsigned CoreTestBase_MockHandleInfo::GetEndReadDataCallCount() const {
  base::AutoLock locker(lock_);
  return end_read_data_call_count_;
}

void CoreTestBase_MockHandleInfo::IncrementCtorCallCount() {
  base::AutoLock locker(lock_);
  ctor_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementDtorCallCount() {
  base::AutoLock locker(lock_);
  dtor_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementCloseCallCount() {
  base::AutoLock locker(lock_);
  close_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementWriteMessageCallCount() {
  base::AutoLock locker(lock_);
  write_message_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementReadMessageCallCount() {
  base::AutoLock locker(lock_);
  read_message_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementWriteDataCallCount() {
  base::AutoLock locker(lock_);
  write_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementBeginWriteDataCallCount() {
  base::AutoLock locker(lock_);
  begin_write_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementEndWriteDataCallCount() {
  base::AutoLock locker(lock_);
  end_write_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementReadDataCallCount() {
  base::AutoLock locker(lock_);
  read_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementBeginReadDataCallCount() {
  base::AutoLock locker(lock_);
  begin_read_data_call_count_++;
}

void CoreTestBase_MockHandleInfo::IncrementEndReadDataCallCount() {
  base::AutoLock locker(lock_);
  end_read_data_call_count_++;
}

}  // namespace test
}  // namespace core
}  // namespace mojo
