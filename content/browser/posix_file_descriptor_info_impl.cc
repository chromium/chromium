// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/posix_file_descriptor_info_impl.h"

#include <utility>

#include "base/stl_util.h"

namespace content {

// static
std::unique_ptr<PosixFileDescriptorInfo> PosixFileDescriptorInfoImpl::Create() {
  return std::unique_ptr<PosixFileDescriptorInfo>(
      new PosixFileDescriptorInfoImpl());
}

PosixFileDescriptorInfoImpl::PosixFileDescriptorInfoImpl() {}

PosixFileDescriptorInfoImpl::~PosixFileDescriptorInfoImpl() {}

void PosixFileDescriptorInfoImpl::Share(int id, base::PlatformFile fd) {
  ShareWithRegion(id, fd, base::MemoryMappedFile::Region::kWholeFile);
}

void PosixFileDescriptorInfoImpl::ShareWithRegion(
    int id,
    base::PlatformFile fd,
    const base::MemoryMappedFile::Region& region) {
  AddToMapping(id, fd, region);
}

void PosixFileDescriptorInfoImpl::Transfer(int id, base::ScopedFD fd) {
  AddToMapping(id, fd.get(), base::MemoryMappedFile::Region::kWholeFile);
  owned_descriptors_.push_back(std::move(fd));
}

base::PlatformFile PosixFileDescriptorInfoImpl::GetFDAt(size_t i) {
  return mapping_[i].first;
}

int PosixFileDescriptorInfoImpl::GetIDAt(size_t i) {
  return mapping_[i].second;
}

const base::MemoryMappedFile::Region& PosixFileDescriptorInfoImpl::GetRegionAt(
    size_t i) {
  auto iter = ids_to_regions_.find(GetIDAt(i));
  return (iter != ids_to_regions_.end())
             ? iter->second
             : base::MemoryMappedFile::Region::kWholeFile;
}

size_t PosixFileDescriptorInfoImpl::GetMappingSize() {
  return mapping_.size();
}

bool PosixFileDescriptorInfoImpl::HasID(int id) const {
  for (unsigned i = 0; i < mapping_.size(); ++i) {
    if (mapping_[i].second == id)
      return true;
  }

  return false;
}

bool PosixFileDescriptorInfoImpl::OwnsFD(base::PlatformFile file) {
  return base::Contains(owned_descriptors_, file);
}

base::ScopedFD PosixFileDescriptorInfoImpl::ReleaseFD(base::PlatformFile file) {
  DCHECK(OwnsFD(file));

  base::ScopedFD fd;
  auto found =
      std::find(owned_descriptors_.begin(), owned_descriptors_.end(), file);

  std::swap(*found, fd);
  owned_descriptors_.erase(found);

  return fd;
}

void PosixFileDescriptorInfoImpl::AddToMapping(
    int id,
    base::PlatformFile fd,
    const base::MemoryMappedFile::Region& region) {
  DCHECK(!HasID(id));
  mapping_.push_back(std::make_pair(fd, id));
  if (region != base::MemoryMappedFile::Region::kWholeFile)
    ids_to_regions_[id] = region;
}

const base::FileHandleMappingVector& PosixFileDescriptorInfoImpl::GetMapping() {
  return mapping_;
}

base::FileHandleMappingVector
PosixFileDescriptorInfoImpl::GetMappingWithIDAdjustment(int delta) {
  base::FileHandleMappingVector result(mapping_);

  // Adding delta to each ID.
  for (size_t i = 0; i < result.size(); ++i)
    result[i].second += delta;
  return result;
}

}  // namespace content
