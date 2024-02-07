// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_in_memory.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace network {

SharedDictionaryInMemory::SharedDictionaryInMemory(
    scoped_refptr<net::IOBuffer> data,
    size_t size,
    const net::SHA256HashValue& sha256,
    const std::string& id)
    : data_(std::move(data)), size_(size), sha256_(sha256), id_(id) {}

SharedDictionaryInMemory::~SharedDictionaryInMemory() = default;

int SharedDictionaryInMemory::ReadAll(base::OnceCallback<void(int)> callback) {
#if DCHECK_IS_ON()
  read_all_called_ = true;
#endif  // DCHECK_IS_ON()
  return net::OK;
}

scoped_refptr<net::IOBuffer> SharedDictionaryInMemory::data() const {
#if DCHECK_IS_ON()
  DCHECK(read_all_called_);
#endif  // DCHECK_IS_ON()
  return data_;
}

size_t SharedDictionaryInMemory::size() const {
  return size_;
}

const net::SHA256HashValue& SharedDictionaryInMemory::hash() const {
  return sha256_;
}

const std::string& SharedDictionaryInMemory::id() const {
  return id_;
}

}  // namespace network
