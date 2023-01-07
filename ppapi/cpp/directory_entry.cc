// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/directory_entry.h"

#include <string.h>

#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"

namespace pp {

DirectoryEntry::DirectoryEntry() {
  memset(&data_, 0, sizeof(data_));
}

DirectoryEntry::DirectoryEntry(
    PassRef, const PP_DirectoryEntry& data) {
  data_.file_ref = data.file_ref;
  data_.file_type = data.file_type;
}

DirectoryEntry::DirectoryEntry(const DirectoryEntry& other) {
  data_.file_ref = other.data_.file_ref;
  data_.file_type = other.data_.file_type;
  if (data_.file_ref)
    Module::Get()->core()->AddRefResource(data_.file_ref);
}

DirectoryEntry::~DirectoryEntry() {
  if (data_.file_ref)
    Module::Get()->core()->ReleaseResource(data_.file_ref);
}

DirectoryEntry& DirectoryEntry::operator=(
    const DirectoryEntry& other) {
  if (data_.file_ref)
    Module::Get()->core()->ReleaseResource(data_.file_ref);
  data_ = other.data_;
  if (data_.file_ref)
    Module::Get()->core()->AddRefResource(data_.file_ref);
  return *this;
}

namespace internal {

DirectoryEntryArrayOutputAdapterWithStorage::
    DirectoryEntryArrayOutputAdapterWithStorage() {
  set_output(&temp_storage_);
}

DirectoryEntryArrayOutputAdapterWithStorage::
    ~DirectoryEntryArrayOutputAdapterWithStorage() {
  if (!temp_storage_.empty()) {
    // An easy way to release the resource references held by |temp_storage_|.
    // A destructor for PP_DirectoryEntry will release them.
    output();
  }
}

std::vector<DirectoryEntry>&
    DirectoryEntryArrayOutputAdapterWithStorage::output() {
  PP_DCHECK(output_storage_.empty());
  typedef std::vector<PP_DirectoryEntry> Entries;
  for (Entries::iterator it = temp_storage_.begin();
       it != temp_storage_.end();
       ++it) {
    output_storage_.push_back(DirectoryEntry(PASS_REF, *it));
  }
  temp_storage_.clear();
  return output_storage_;
}

}  // namespace internal
}  // namespace pp
