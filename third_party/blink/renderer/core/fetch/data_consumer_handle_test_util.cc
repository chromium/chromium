// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/data_consumer_handle_test_util.h"

#include <memory>
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

namespace {

class WaitingHandle final : public WebDataConsumerHandle {
 private:
  class ReaderImpl final : public WebDataConsumerHandle::Reader {
   public:
    WebDataConsumerHandle::Result BeginRead(const void** buffer,
                                            WebDataConsumerHandle::Flags,
                                            size_t* available) override {
      *available = 0;
      *buffer = nullptr;
      return kShouldWait;
    }
    WebDataConsumerHandle::Result EndRead(size_t) override {
      return kUnexpectedError;
    }
  };
  std::unique_ptr<Reader> ObtainReader(
      Client*,
      scoped_refptr<base::SingleThreadTaskRunner>) override {
    return std::make_unique<ReaderImpl>();
  }

  const char* DebugName() const override { return "WaitingHandle"; }
};

}  // namespace

DataConsumerHandleTestUtil::Thread::Thread(
    const ThreadCreationParams& params,
    InitializationPolicy initialization_policy)
    : thread_(WebThreadSupportingGC::Create(params)),
      initialization_policy_(initialization_policy),
      waitable_event_(std::make_unique<WaitableEvent>()) {
  thread_->PostTask(FROM_HERE, CrossThreadBind(&Thread::Initialize,
                                               CrossThreadUnretained(this)));
  waitable_event_->Wait();
}

DataConsumerHandleTestUtil::Thread::~Thread() {
  thread_->PostTask(FROM_HERE, CrossThreadBind(&Thread::Shutdown,
                                               CrossThreadUnretained(this)));
  waitable_event_->Wait();
}

void DataConsumerHandleTestUtil::Thread::Initialize() {
  DCHECK(thread_->IsCurrentThread());
  if (initialization_policy_ >= kScriptExecution) {
    isolate_holder_ = std::make_unique<gin::IsolateHolder>(
        scheduler::GetSingleThreadTaskRunnerForTesting(),
        gin::IsolateHolder::IsolateType::kTest);
    GetIsolate()->Enter();
  }
  thread_->InitializeOnThread();
  if (initialization_policy_ >= kScriptExecution) {
    v8::HandleScope handle_scope(GetIsolate());
    script_state_ = ScriptState::Create(
        v8::Context::New(GetIsolate()),
        DOMWrapperWorld::Create(GetIsolate(),
                                DOMWrapperWorld::WorldType::kTesting));
  }
  if (initialization_policy_ >= kWithExecutionContext) {
    execution_context_ = new NullExecutionContext();
  }
  waitable_event_->Signal();
}

void DataConsumerHandleTestUtil::Thread::Shutdown() {
  DCHECK(thread_->IsCurrentThread());
  execution_context_ = nullptr;
  if (script_state_) {
    script_state_->DisposePerContextData();
  }
  script_state_ = nullptr;
  thread_->ShutdownOnThread();
  if (isolate_holder_) {
    GetIsolate()->Exit();
    GetIsolate()->RequestGarbageCollectionForTesting(
        GetIsolate()->kFullGarbageCollection);
    isolate_holder_ = nullptr;
  }
  waitable_event_->Signal();
}

class DataConsumerHandleTestUtil::ReplayingHandle::ReaderImpl final
    : public Reader {
 public:
  ReaderImpl(scoped_refptr<Context> context, Client* client)
      : context_(std::move(context)) {
    context_->AttachReader(client);
  }
  ~ReaderImpl() override { context_->DetachReader(); }

  WebDataConsumerHandle::Result BeginRead(const void** buffer,
                                          Flags flags,
                                          size_t* available) override {
    return context_->BeginRead(buffer, flags, available);
  }
  WebDataConsumerHandle::Result EndRead(size_t read_size) override {
    return context_->EndRead(read_size);
  }

 private:
  scoped_refptr<Context> context_;
};

void DataConsumerHandleTestUtil::ReplayingHandle::Context::Add(
    const Command& command) {
  MutexLocker locker(mutex_);
  commands_.push_back(command);
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::AttachReader(
    WebDataConsumerHandle::Client* client) {
  MutexLocker locker(mutex_);
  DCHECK(!reader_thread_);
  DCHECK(!client_);
  reader_thread_ = Platform::Current()->CurrentThread();
  client_ = client;

  if (client_ && !(IsEmpty() && result_ == kShouldWait))
    Notify();
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::DetachReader() {
  MutexLocker locker(mutex_);
  DCHECK(reader_thread_ && reader_thread_->IsCurrentThread());
  reader_thread_ = nullptr;
  client_ = nullptr;
  if (!is_handle_attached_)
    detached_->Signal();
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::DetachHandle() {
  MutexLocker locker(mutex_);
  is_handle_attached_ = false;
  if (!reader_thread_)
    detached_->Signal();
}

WebDataConsumerHandle::Result
DataConsumerHandleTestUtil::ReplayingHandle::Context::BeginRead(
    const void** buffer,
    Flags,
    size_t* available) {
  MutexLocker locker(mutex_);
  *buffer = nullptr;
  *available = 0;
  if (IsEmpty())
    return result_;

  const Command& command = Top();
  WebDataConsumerHandle::Result result = kOk;
  switch (command.GetName()) {
    case Command::kData: {
      auto& body = command.Body();
      *available = body.size() - Offset();
      *buffer = body.data() + Offset();
      result = kOk;
      break;
    }
    case Command::kDone:
      result_ = result = kDone;
      Consume(0);
      break;
    case Command::kWait:
      Consume(0);
      result = kShouldWait;
      Notify();
      break;
    case Command::kError:
      result_ = result = kUnexpectedError;
      Consume(0);
      break;
  }
  return result;
}

WebDataConsumerHandle::Result
DataConsumerHandleTestUtil::ReplayingHandle::Context::EndRead(
    size_t read_size) {
  MutexLocker locker(mutex_);
  Consume(read_size);
  return kOk;
}

DataConsumerHandleTestUtil::ReplayingHandle::Context::Context()
    : offset_(0),
      reader_thread_(nullptr),
      client_(nullptr),
      result_(kShouldWait),
      is_handle_attached_(true),
      detached_(std::make_unique<WaitableEvent>()) {}

const DataConsumerHandleTestUtil::Command&
DataConsumerHandleTestUtil::ReplayingHandle::Context::Top() {
  DCHECK(!IsEmpty());
  return commands_.front();
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::Consume(
    size_t size) {
  DCHECK(!IsEmpty());
  DCHECK(size + offset_ <= Top().Body().size());
  bool fully_consumed = (size + offset_ >= Top().Body().size());
  if (fully_consumed) {
    offset_ = 0;
    commands_.pop_front();
  } else {
    offset_ += size;
  }
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::Notify() {
  if (!client_)
    return;
  DCHECK(reader_thread_);
  PostCrossThreadTask(
      *reader_thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBind(&Context::NotifyInternal, WrapRefCounted(this)));
}

void DataConsumerHandleTestUtil::ReplayingHandle::Context::NotifyInternal() {
  {
    MutexLocker locker(mutex_);
    if (!client_ || !reader_thread_->IsCurrentThread()) {
      // There is no client, or a new reader is attached.
      return;
    }
  }
  // The reading thread is the current thread.
  client_->DidGetReadable();
}

DataConsumerHandleTestUtil::ReplayingHandle::ReplayingHandle()
    : context_(Context::Create()) {}

DataConsumerHandleTestUtil::ReplayingHandle::~ReplayingHandle() {
  context_->DetachHandle();
}

std::unique_ptr<WebDataConsumerHandle::Reader>
DataConsumerHandleTestUtil::ReplayingHandle::ObtainReader(
    Client* client,
    scoped_refptr<base::SingleThreadTaskRunner>) {
  return std::make_unique<ReaderImpl>(context_, client);
}

void DataConsumerHandleTestUtil::ReplayingHandle::Add(const Command& command) {
  context_->Add(command);
}

std::unique_ptr<WebDataConsumerHandle>
DataConsumerHandleTestUtil::CreateWaitingDataConsumerHandle() {
  return std::make_unique<WaitingHandle>();
}

}  // namespace blink
