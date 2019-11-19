// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_POSIX_FILE_DESCRIPTOR_INFO_H_
#define CONTENT_PUBLIC_BROWSER_POSIX_FILE_DESCRIPTOR_INFO_H_

#include <stddef.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/process/launch.h"

namespace content {

// PoxisFileDescriptorInfo is a collection of file descriptors which is needed
// to launch a process. You should tell PosixFileDescriptorInfo which FDs
// should be closed and which shouldn't so that it can take care of the
// lifetime of FDs.
//
// See base/process/launcher.h for more details about launching a process.
class PosixFileDescriptorInfo {
 public:
  virtual ~PosixFileDescriptorInfo() {}

  // Adds an FD associated with an ID, without delegating the ownerhip of ID.
  virtual void Share(int id, base::PlatformFile fd) = 0;

  // Similar to Share but also provides a region in that file that should be
  // read in the launched process (accessible with GetRegionAt()).
  virtual void ShareWithRegion(
      int id,
      base::PlatformFile fd,
      const base::MemoryMappedFile::Region& region) = 0;

  // Adds an FD associated with an ID, passing the FD ownership to
  // FileDescriptorInfo.
  virtual void Transfer(int id, base::ScopedFD fd) = 0;

  // A vector backed map of registered ID-FD pairs.
  virtual const base::FileHandleMappingVector& GetMapping() = 0;

  // A GetMapping() variant that adjusts the ID value by |delta|.
  // Some environments need this trick.
  virtual base::FileHandleMappingVector GetMappingWithIDAdjustment(
      int delta) = 0;

  // API for iterating over the registered ID-FD pairs.
  virtual base::PlatformFile GetFDAt(size_t i) = 0;
  virtual int GetIDAt(size_t i) = 0;
  virtual const base::MemoryMappedFile::Region& GetRegionAt(size_t i) = 0;
  virtual size_t GetMappingSize() = 0;

  // Returns true if |this| has ownership of |file|.
  virtual bool OwnsFD(base::PlatformFile file) = 0;
  // Assuming |OwnsFD(file)|, releases the ownership.
  virtual base::ScopedFD ReleaseFD(base::PlatformFile file) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_POSIX_FILE_DESCRIPTOR_INFO_H_
