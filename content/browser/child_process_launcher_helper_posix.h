// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_POSIX_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_POSIX_H_

#include <map>
#include <memory>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace mojo {
class PlatformChannelEndpoint;
}  // namespace mojo

// Contains the common functionalities between the various POSIX child process
// launcher implementations.

namespace content {

class PosixFileDescriptorInfo;

namespace internal {

std::unique_ptr<PosixFileDescriptorInfo> CreateDefaultPosixFilesToMap(
    int child_process_id,
    const mojo::PlatformChannelEndpoint& mojo_channel_remote_endpoint,
    const std::map<std::string, absl::variant<base::FilePath, base::ScopedFD>>&
        files_to_preload,
    const std::string& process_type,
    base::CommandLine* command_line);

// Opens the file in read mode at the given path. Note that the path should be
// relative and the way it is resolved is platform specific.
// |region| is set to the region of the file that should be read.
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region);

}  // namespace internal

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_POSIX_H_
