// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

namespace blink {

namespace {

class ErroredBytesConsumer final : public BytesConsumer {
 public:
  explicit ErroredBytesConsumer(const Error& error) : error_(error) {}

  Result BeginRead(base::span<const char>& buffer) override {
    buffer = {};
    return Result::kError;
  }
  Result EndRead(size_t read_size) override {
    NOTREACHED_IN_MIGRATION();
    return Result::kError;
  }
  void SetClient(BytesConsumer::Client*) override {}
  void ClearClient() override {}

  void Cancel() override {}
  PublicState GetPublicState() const override { return PublicState::kErrored; }
  Error GetError() const override { return error_; }
  String DebugName() const override { return "ErroredBytesConsumer"; }

 private:
  const Error error_;
};

class ClosedBytesConsumer final : public BytesConsumer {
 public:
  Result BeginRead(base::span<const char>& buffer) override {
    buffer = {};
    return Result::kDone;
  }
  Result EndRead(size_t read_size) override {
    NOTREACHED_IN_MIGRATION();
    return Result::kError;
  }
  void SetClient(BytesConsumer::Client*) override {}
  void ClearClient() override {}

  void Cancel() override {}
  PublicState GetPublicState() const override { return PublicState::kClosed; }
  Error GetError() const override {
    NOTREACHED_IN_MIGRATION();
    return Error();
  }
  String DebugName() const override { return "ClosedBytesConsumer"; }
};

}  // namespace

BytesConsumer::Result BytesConsumer::BeginRead(const char** buffer,
                                               size_t* available) {
  base::span<const char> buffer_span;
  auto result = BeginRead(buffer_span);
  *buffer = buffer_span.data();
  *available = buffer_span.size();
  return result;
}

BytesConsumer* BytesConsumer::CreateErrored(const BytesConsumer::Error& error) {
  return MakeGarbageCollected<ErroredBytesConsumer>(error);
}

BytesConsumer* BytesConsumer::CreateClosed() {
  return MakeGarbageCollected<ClosedBytesConsumer>();
}

std::ostream& operator<<(std::ostream& out,
                         const BytesConsumer::PublicState& state) {
  switch (state) {
    case BytesConsumer::PublicState::kReadableOrWaiting:
      return out << "kReadableOrWaiting";
    case BytesConsumer::PublicState::kClosed:
      return out << "kClosed";
    case BytesConsumer::PublicState::kErrored:
      return out << "kErrored";
  }
  NOTREACHED_IN_MIGRATION();
}

std::ostream& operator<<(std::ostream& out,
                         const BytesConsumer::Result& result) {
  switch (result) {
    case BytesConsumer::Result::kOk:
      return out << "kOk";
    case BytesConsumer::Result::kShouldWait:
      return out << "kShouldWait";
    case BytesConsumer::Result::kDone:
      return out << "kDone";
    case BytesConsumer::Result::kError:
      return out << "kError";
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace blink
