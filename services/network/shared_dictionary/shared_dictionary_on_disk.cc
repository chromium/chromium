// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_on_disk.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "services/network/shared_dictionary/shared_dictionary_disk_cache.h"

namespace network {
namespace {

constexpr char kHistogramPrefix[] = "Net.SharedDictionaryOnDisk.";

}  // namespace

SharedDictionaryOnDisk::SharedDictionaryOnDisk(
    size_t size,
    const net::SHA256HashValue& hash,
    const std::string& id,
    const base::UnguessableToken& disk_cache_key_token,
    SharedDictionaryDiskCache& disk_cahe,
    base::OnceClosure disk_cache_error_callback,
    base::ScopedClosureRunner on_deleted_closure_runner)
    : size_(size),
      hash_(hash),
      id_(id),
      disk_cache_error_callback_(std::move(disk_cache_error_callback)),
      on_deleted_closure_runner_(std::move(on_deleted_closure_runner)) {
  auto split_callback = base::SplitOnceCallback(base::BindOnce(
      &SharedDictionaryOnDisk::OnEntry, weak_factory_.GetWeakPtr(),
      /*open_start_time=*/base::Time::Now()));
  disk_cache::EntryResult result = disk_cahe.OpenOrCreateEntry(
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

const std::string& SharedDictionaryOnDisk::id() const {
  return id_;
}

void SharedDictionaryOnDisk::OnEntry(base::Time open_start_time,
                                     disk_cache::EntryResult result) {
  bool succeeded = result.net_error() == net::OK;
  base::Time now = base::Time::Now();
  base::UmaHistogramTimes(base::StrCat({kHistogramPrefix, "OpenEntryLatency.",
                                        succeeded ? "Success" : "Failure"}),
                          now - open_start_time);

  if (!succeeded) {
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

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&SharedDictionaryOnDisk::OnDataRead,
                     weak_factory_.GetWeakPtr(), /*read_start_time=*/now));

  int rv = entry_->ReadData(/*index=*/1,
                            /*offset=*/0, data_.get(), size_,
                            std::move(split_callback.first));
  if (rv != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(rv);
  }
}

void SharedDictionaryOnDisk::OnDataRead(base::Time read_start_time,
                                        int result) {
  bool succeeded = result >= 0 && (base::checked_cast<size_t>(result) == size_);
  base::UmaHistogramTimes(base::StrCat({kHistogramPrefix, "ReadDataLatency.",
                                        succeeded ? "Success" : "Failure"}),
                          base::Time::Now() - read_start_time);
  entry_.reset();
  SetState(succeeded ? State::kDone : State::kFailed);
}

void SharedDictionaryOnDisk::SetState(State state) {
  CHECK_NE(State::kLoading, state);
  CHECK_EQ(State::kLoading, state_);
  state_ = state;

  if (state_ == State::kFailed && disk_cache_error_callback_) {
    std::move(disk_cache_error_callback_).Run();
  }
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
