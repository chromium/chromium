// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_writer_on_disk.h"

#include <limits>

#include "base/functional/callback_helpers.h"
#include "base/numerics/checked_math.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"

namespace network {

SharedDictionaryWriterOnDisk::SharedDictionaryWriterOnDisk(
    const base::UnguessableToken& token,
    FinishCallback callback,
    base::WeakPtr<SharedDictionaryDiskCache> disk_cahe)
    : token_(token),
      callback_(std::move(callback)),
      disk_cahe_(disk_cahe),
      secure_hash_(crypto::SecureHash::Create(crypto::SecureHash::SHA256)) {}

SharedDictionaryWriterOnDisk::~SharedDictionaryWriterOnDisk() {
  if (callback_) {
    OnFailed(Result::kErrorAborted);
  }
}

void SharedDictionaryWriterOnDisk::Initialize() {
  DCHECK_EQ(State::kBeforeInitialize, state_);
  state_ = State::kInitializing;
  DCHECK(disk_cahe_);
  // Binding `this` to keep `this` alive until callback will be called.
  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&SharedDictionaryWriterOnDisk::OnEntry, this));
  disk_cache::EntryResult result = disk_cahe_->OpenOrCreateEntry(
      token_.ToString(), /*create=*/true, std::move(split_callback.first));
  if (result.net_error() != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(std::move(result));
  }
}

void SharedDictionaryWriterOnDisk::Append(const char* buf, int num_bytes) {
  DCHECK_GT(num_bytes, 0);
  if (state_ == State::kFailed) {
    return;
  }
  base::CheckedNumeric<size_t> checked_total_size = total_size_;
  checked_total_size += num_bytes;
  if (checked_total_size.ValueOrDefault(std::numeric_limits<size_t>::max()) >
      shared_dictionary::GetDictionarySizeLimit()) {
    OnFailed(Result::kErrorSizeExceedsLimit);
    return;
  }

  total_size_ = checked_total_size.ValueOrDie();
  secure_hash_->Update(buf, num_bytes);
  switch (state_) {
    case State::kBeforeInitialize:
      NOTREACHED_IN_MIGRATION();
      return;
    case State::kInitializing:
      pending_write_buffers_.push_back(
          base::MakeRefCounted<net::StringIOBuffer>(
              std::string(buf, num_bytes)));
      break;
    case State::kInitialized: {
      DCHECK(entry_);
      WriteData(base::MakeRefCounted<net::StringIOBuffer>(
          std::string(buf, num_bytes)));
    } break;
    case State::kFailed:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void SharedDictionaryWriterOnDisk::Finish() {
  if (state_ == State::kFailed) {
    return;
  }

  finish_called_ = true;
  MaybeFinish();
}

void SharedDictionaryWriterOnDisk::OnEntry(disk_cache::EntryResult result) {
  if (state_ != State::kInitializing) {
    DCHECK_EQ(State::kFailed, state_);
    return;
  }
  if (result.net_error() != net::OK) {
    OnFailed(Result::kErrorCreateEntryFailed);
    return;
  }
  state_ = State::kInitialized;
  entry_.reset(result.ReleaseEntry());

  if (pending_write_buffers_.empty()) {
    MaybeFinish();
    return;
  }

  while (!pending_write_buffers_.empty()) {
    scoped_refptr<net::StringIOBuffer> buffer = *pending_write_buffers_.begin();
    pending_write_buffers_.pop_front();
    WriteData(std::move(buffer));
  }
}

void SharedDictionaryWriterOnDisk::WriteData(
    scoped_refptr<net::StringIOBuffer> buffer) {
  DCHECK_NE(State::kBeforeInitialize, state_);
  DCHECK_NE(State::kInitializing, state_);
  if (state_ != State::kInitialized) {
    DCHECK_EQ(State::kFailed, state_);
    return;
  }
  offset_ += buffer->size();
  ++writing_count_;
  // Binding `this` to keep `this` alive until callback will be called.
  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &SharedDictionaryWriterOnDisk::OnWrittenData, this, buffer->size()));
  // Stores the dictionary binary in the second stream of disk cache entry
  // (index = 1) which was designed to store the HTTP response body of HTTP
  // Cache.
  int result = entry_->WriteData(
      /*index=*/1, /*offset=*/offset_ - buffer->size(), buffer.get(),
      buffer->size(), std::move(split_callback.first), false);
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(std::move(result));
  }
}

void SharedDictionaryWriterOnDisk::OnWrittenData(int expected_result,
                                                 int result) {
  DCHECK_NE(State::kBeforeInitialize, state_);
  DCHECK_NE(State::kInitializing, state_);
  if (state_ != State::kInitialized) {
    DCHECK_EQ(State::kFailed, state_);
    return;
  }
  if (result != expected_result) {
    OnFailed(Result::kErrorWriteDataFailed);
    return;
  }
  written_size_ += result;
  --writing_count_;
  MaybeFinish();
}

void SharedDictionaryWriterOnDisk::OnFailed(Result result) {
  DCHECK_NE(State::kFailed, state_);
  state_ = State::kFailed;
  pending_write_buffers_.clear();
  if (entry_) {
    entry_->Doom();
    entry_.reset();
  }
  std::move(callback_).Run(result, 0u, net::SHA256HashValue());
}

void SharedDictionaryWriterOnDisk::MaybeFinish() {
  if ((writing_count_ != 0) || !finish_called_ || !callback_ ||
      !pending_write_buffers_.empty() || (state_ != State::kInitialized)) {
    return;
  }

  if (total_size_ == 0) {
    OnFailed(Result::kErrorSizeZero);
    return;
  }

  entry_.reset();
  DCHECK_EQ(written_size_, total_size_);
  net::SHA256HashValue sha256;
  secure_hash_->Finish(sha256.data, sizeof(sha256.data));
  std::move(callback_).Run(Result::kSuccess, total_size_, sha256);
}

}  // namespace network
