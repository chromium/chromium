// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/bytes_consumer_tee.h"

#include <string.h>

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class NoopClient final : public GarbageCollected<NoopClient>,
                         public BytesConsumer::Client {
 public:
  void OnStateChange() override {}
  String DebugName() const override { return "NoopClient"; }
};

class TeeHelper final : public GarbageCollected<TeeHelper>,
                        public BytesConsumer::Client {
 public:
  TeeHelper(ExecutionContext* execution_context, BytesConsumer* consumer)
      : src_(consumer),
        destination1_(
            MakeGarbageCollected<Destination>(execution_context, this)),
        destination2_(
            MakeGarbageCollected<Destination>(execution_context, this)) {
    consumer->SetClient(this);
    // As no client is set to either destinations, Destination::notify() is
    // no-op in this function.
    OnStateChange();
  }

  void OnStateChange() override {
    bool destination1_was_empty = destination1_->IsEmpty();
    bool destination2_was_empty = destination2_->IsEmpty();
    bool has_enqueued = false;

    while (true) {
      base::span<const char> buffer;
      auto result = src_->BeginRead(buffer);
      if (result == Result::kShouldWait) {
        if (has_enqueued && destination1_was_empty)
          destination1_->Notify();
        if (has_enqueued && destination2_was_empty)
          destination2_->Notify();
        return;
      }
      Chunk* chunk = nullptr;
      if (result == Result::kOk) {
        chunk = MakeGarbageCollected<Chunk>(buffer);
        result = src_->EndRead(buffer.size());
      }
      switch (result) {
        case Result::kOk:
          DCHECK(chunk);
          destination1_->Enqueue(chunk);
          destination2_->Enqueue(chunk);
          has_enqueued = true;
          break;
        case Result::kShouldWait:
          NOTREACHED_IN_MIGRATION();
          return;
        case Result::kDone:
          if (chunk) {
            destination1_->Enqueue(chunk);
            destination2_->Enqueue(chunk);
          }
          if (destination1_was_empty)
            destination1_->Notify();
          if (destination2_was_empty)
            destination2_->Notify();
          return;
        case Result::kError:
          ClearAndNotify();
          return;
      }
    }
  }
  String DebugName() const override { return "TeeHelper"; }

  BytesConsumer::PublicState GetPublicState() const {
    return src_->GetPublicState();
  }

  BytesConsumer::Error GetError() const { return src_->GetError(); }

  void Cancel() {
    if (!destination1_->IsCancelled() || !destination2_->IsCancelled())
      return;
    src_->Cancel();
  }

  BytesConsumer* Destination1() const { return destination1_.Get(); }
  BytesConsumer* Destination2() const { return destination2_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(src_);
    visitor->Trace(destination1_);
    visitor->Trace(destination2_);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  using Result = BytesConsumer::Result;
  class Chunk final : public GarbageCollected<Chunk> {
   public:
    explicit Chunk(base::span<const char> data) {
      buffer_.ReserveInitialCapacity(
          base::checked_cast<wtf_size_t>(data.size()));
      buffer_.AppendSpan(data);
      // Report buffer size to V8 so GC can be triggered appropriately.
      external_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                          static_cast<int64_t>(buffer_.size()));
    }
    ~Chunk() {
      external_memory_accounter_.Decrease(v8::Isolate::GetCurrent(),
                                          static_cast<int64_t>(buffer_.size()));
    }
    const char* data() const { return buffer_.data(); }
    wtf_size_t size() const { return buffer_.size(); }

    void Trace(Visitor* visitor) const {}

   private:
    Vector<char> buffer_;
    NO_UNIQUE_ADDRESS V8ExternalMemoryAccounterBase external_memory_accounter_;
  };

  class Destination final : public BytesConsumer {
   public:
    Destination(ExecutionContext* execution_context, TeeHelper* tee)
        : execution_context_(execution_context), tee_(tee) {}

    Result BeginRead(base::span<const char>& buffer) override {
      DCHECK(!chunk_in_use_);
      buffer = {};
      if (is_cancelled_ || is_closed_)
        return Result::kDone;
      if (!chunks_.empty()) {
        Chunk* chunk = chunks_[0];
        DCHECK_LE(offset_, chunk->size());
        buffer = base::span(*chunk).subspan(offset_);
        chunk_in_use_ = chunk;
        return Result::kOk;
      }
      switch (tee_->GetPublicState()) {
        case PublicState::kReadableOrWaiting:
          return Result::kShouldWait;
        case PublicState::kClosed:
          is_closed_ = true;
          ClearClient();
          return Result::kDone;
        case PublicState::kErrored:
          ClearClient();
          return Result::kError;
      }
      NOTREACHED_IN_MIGRATION();
      return Result::kError;
    }

    Result EndRead(size_t read) override {
      DCHECK(chunk_in_use_);
      DCHECK(chunks_.empty() || chunk_in_use_ == chunks_[0]);
      chunk_in_use_ = nullptr;
      if (chunks_.empty()) {
        // This object becomes errored during the two-phase read.
        DCHECK_EQ(PublicState::kErrored, GetPublicState());
        return Result::kOk;
      }
      Chunk* chunk = chunks_[0];
      DCHECK_LE(offset_ + read, chunk->size());
      offset_ += read;
      if (chunk->size() == offset_) {
        offset_ = 0;
        chunks_.pop_front();
      }
      if (chunks_.empty() && tee_->GetPublicState() == PublicState::kClosed) {
        // All data has been consumed.
        execution_context_->GetTaskRunner(TaskType::kNetworking)
            ->PostTask(FROM_HERE, WTF::BindOnce(&Destination::Close,
                                                WrapPersistent(this)));
      }
      return Result::kOk;
    }

    void SetClient(BytesConsumer::Client* client) override {
      DCHECK(!client_);
      DCHECK(client);
      auto state = GetPublicState();
      if (state == PublicState::kClosed || state == PublicState::kErrored)
        return;
      client_ = client;
    }

    void ClearClient() override { client_ = nullptr; }

    void Cancel() override {
      DCHECK(!chunk_in_use_);
      auto state = GetPublicState();
      if (state == PublicState::kClosed || state == PublicState::kErrored)
        return;
      is_cancelled_ = true;
      ClearChunks();
      ClearClient();
      tee_->Cancel();
    }

    PublicState GetPublicState() const override {
      if (is_cancelled_ || is_closed_)
        return PublicState::kClosed;
      auto state = tee_->GetPublicState();
      // We don't say this object is closed unless m_isCancelled or
      // m_isClosed is set.
      return state == PublicState::kClosed ? PublicState::kReadableOrWaiting
                                           : state;
    }

    Error GetError() const override { return tee_->GetError(); }

    String DebugName() const override { return "TeeHelper::Destination"; }

    void Enqueue(Chunk* chunk) {
      if (is_cancelled_)
        return;
      chunks_.push_back(chunk);
    }

    bool IsEmpty() const { return chunks_.empty(); }

    void ClearChunks() {
      chunks_.clear();
      offset_ = 0;
    }

    void Notify() {
      if (is_cancelled_ || is_closed_)
        return;
      if (chunks_.empty() && tee_->GetPublicState() == PublicState::kClosed) {
        Close();
        return;
      }
      if (client_) {
        client_->OnStateChange();
        if (GetPublicState() == PublicState::kErrored)
          ClearClient();
      }
    }

    bool IsCancelled() const { return is_cancelled_; }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(execution_context_);
      visitor->Trace(tee_);
      visitor->Trace(client_);
      visitor->Trace(chunks_);
      visitor->Trace(chunk_in_use_);
      BytesConsumer::Trace(visitor);
    }

   private:
    void Close() {
      DCHECK_EQ(PublicState::kClosed, tee_->GetPublicState());
      DCHECK(chunks_.empty());
      if (is_closed_ || is_cancelled_) {
        // It's possible to reach here because this function can be
        // called asynchronously.
        return;
      }
      DCHECK_EQ(PublicState::kReadableOrWaiting, GetPublicState());
      is_closed_ = true;
      if (client_) {
        client_->OnStateChange();
        ClearClient();
      }
    }

    Member<ExecutionContext> execution_context_;
    Member<TeeHelper> tee_;
    Member<BytesConsumer::Client> client_;
    HeapDeque<Member<Chunk>> chunks_;
    Member<Chunk> chunk_in_use_;
    size_t offset_ = 0;
    bool is_cancelled_ = false;
    bool is_closed_ = false;
  };

  void ClearAndNotify() {
    destination1_->ClearChunks();
    destination2_->ClearChunks();
    destination1_->Notify();
    destination2_->Notify();
  }

  Member<BytesConsumer> src_;
  Member<Destination> destination1_;
  Member<Destination> destination2_;
};

}  // namespace

void BytesConsumerTee(ExecutionContext* execution_context,
                      BytesConsumer* src,
                      BytesConsumer** dest1,
                      BytesConsumer** dest2) {
  scoped_refptr<BlobDataHandle> blob_data_handle = src->DrainAsBlobDataHandle(
      BytesConsumer::BlobSizePolicy::kAllowBlobWithInvalidSize);
  if (blob_data_handle) {
    // Register a client in order to be consistent.
    src->SetClient(MakeGarbageCollected<NoopClient>());
    *dest1 = MakeGarbageCollected<BlobBytesConsumer>(execution_context,
                                                     blob_data_handle);
    *dest2 = MakeGarbageCollected<BlobBytesConsumer>(execution_context,
                                                     blob_data_handle);
    return;
  }

  auto form_data = src->DrainAsFormData();
  if (form_data) {
    // Register a client in order to be consistent.
    src->SetClient(MakeGarbageCollected<NoopClient>());
    *dest1 = MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                         form_data);
    *dest2 = MakeGarbageCollected<FormDataBytesConsumer>(execution_context,
                                                         form_data);
    return;
  }

  TeeHelper* tee = MakeGarbageCollected<TeeHelper>(execution_context, src);
  *dest1 = tee->Destination1();
  *dest2 = tee->Destination2();
}

}  // namespace blink
