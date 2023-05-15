// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_on_disk.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"

namespace network {

SharedDictionaryOnDisk::SharedDictionaryOnDisk(
    size_t size,
    const net::SHA256HashValue& hash,
    const base::UnguessableToken& disk_cache_key_token,
    SharedDictionaryDiskCache* disk_cahe)
    : size_(size), hash_(hash) {
  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &SharedDictionaryOnDisk::OnEntry, weak_factory_.GetWeakPtr()));
  disk_cache::EntryResult result = disk_cahe->OpenOrCreateEntry(
      disk_cache_key_token.ToString(),
      /*create=*/false, std::move(split_callback.first));
  if (result.net_error() != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(std::move(result));
  }
}

SharedDictionaryOnDisk::~SharedDictionaryOnDisk() = default;

int SharedDictionaryOnDisk::ReadAll(base::OnceCallback<void(int)> callback) {
  if (state_ == State::kDone) {
    return net::OK;
  }
  if (state_ == State::kFailed) {
    return net::ERR_FAILED;
  }
  readall_callbacks_.push_back(std::move(callback));
  return net::ERR_IO_PENDING;
}

scoped_refptr<net::IOBuffer> SharedDictionaryOnDisk::data() const {
  CHECK_EQ(State::kDone, state_);
  return data_.get();
}

size_t SharedDictionaryOnDisk::size() const {
  return size_;
}

const net::SHA256HashValue& SharedDictionaryOnDisk::hash() const {
  return hash_;
}

void SharedDictionaryOnDisk::OnEntry(disk_cache::EntryResult result) {
  if (result.net_error() != net::OK) {
    SetState(State::kFailed);
    return;
  }
  entry_.reset(result.ReleaseEntry());
  // The dictionary binary is stored in the second stream of disk cache entry
  // (index = 1).
  if (static_cast<size_t>(entry_->GetDataSize(/*index=*/1)) != size_) {
    SetState(State::kFailed);
    return;
  }
  data_ = base::MakeRefCounted<net::IOBufferWithSize>(size_);

  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &SharedDictionaryOnDisk::OnDataRead, weak_factory_.GetWeakPtr()));

  int rv = entry_->ReadData(/*index=*/1,
                            /*offset=*/0, data_.get(), size_,
                            std::move(split_callback.first));
  if (rv != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(rv);
  }
}

void SharedDictionaryOnDisk::OnDataRead(int result) {
  entry_.reset();

  if (result < 0 || (base::checked_cast<size_t>(result) != size_)) {
    SetState(State::kFailed);
    return;
  }
  SetState(State::kDone);
}

void SharedDictionaryOnDisk::SetState(State state) {
  CHECK_NE(State::kLoading, state);
  CHECK_EQ(State::kLoading, state_);
  state_ = state;
  auto readall_callbacks = std::move(readall_callbacks_);
  for (auto& readall_callback : readall_callbacks) {
    if (state_ == State::kDone) {
      std::move(readall_callback).Run(net::OK);
    } else if (state_ == State::kFailed) {
      std::move(readall_callback).Run(net::ERR_FAILED);
    }
  }
}

}  // namespace network
