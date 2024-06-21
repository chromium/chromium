// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper_posix.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/overloaded.h"
#include "base/metrics/field_trial.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/posix_file_descriptor_info_impl.h"
#include "content/common/shared_file_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {
namespace internal {

namespace {

base::PlatformFile OpenFileIfNecessary(const base::FilePath& path,
                                       base::MemoryMappedFile::Region* region) {
  static auto* opened_files = new std::map<
      base::FilePath,
      std::pair<base::PlatformFile, base::MemoryMappedFile::Region>>;

  const auto& iter = opened_files->find(path);
  if (iter != opened_files->end()) {
    *region = iter->second.second;
    return iter->second.first;
  }
  base::File file = OpenFileToShare(path, region);
  if (!file.IsValid()) {
    return base::kInvalidPlatformFile;
  }
  // g_opened_files becomes the owner of the file descriptor.
  base::PlatformFile fd = file.TakePlatformFile();
  (*opened_files)[path] = std::make_pair(fd, *region);
  return fd;
}

}  // namespace

std::unique_ptr<PosixFileDescriptorInfo> CreateDefaultPosixFilesToMap(
    int child_process_id,
    const mojo::PlatformChannelEndpoint& mojo_channel_remote_endpoint,
    const std::map<std::string, absl::variant<base::FilePath, base::ScopedFD>>&
        files_to_preload,
    const std::string& process_type,
    base::CommandLine* command_line) {
  std::unique_ptr<PosixFileDescriptorInfo> files_to_register(
      PosixFileDescriptorInfoImpl::Create());

// Mac shared memory doesn't use file descriptors.
#if !BUILDFLAG(IS_APPLE)
#if BUILDFLAG(IS_ANDROID)
  // Android's endpoint may be a file descriptor or a binder. If it's a binder
  // we share it by other means.
  const bool share_channel_fd =
      !mojo_channel_remote_endpoint.platform_handle().is_binder();
#else
  const bool share_channel_fd = true;
#endif
  if (share_channel_fd) {
    DCHECK(mojo_channel_remote_endpoint.is_valid());
    files_to_register->Share(
        kMojoIPCChannel,
        mojo_channel_remote_endpoint.platform_handle().GetFD().get());
  }

  // TODO(jcivelli): remove this "if defined" by making
  // GetAdditionalMappedFilesForChildProcess a no op on Mac.
  GetContentClient()->browser()->GetAdditionalMappedFilesForChildProcess(
      *command_line, child_process_id, files_to_register.get());
#endif

  // Also include the files specified explicitly by |files_to_preload|.
  base::GlobalDescriptors::Key key = kContentDynamicDescriptorStart;
  SharedFileSwitchValueBuilder file_switch_value_builder;
  for (const auto& key_path_iter : files_to_preload) {
    base::MemoryMappedFile::Region region;
    base::PlatformFile file = absl::visit(
        base::Overloaded{[&region](const base::FilePath& file_path) {
                           base::PlatformFile file =
                               OpenFileIfNecessary(file_path, &region);
                           if (file == base::kInvalidPlatformFile) {
                             DLOG(WARNING) << "Ignoring invalid file "
                                           << file_path.value();
                           }
                           return file;
                         },
                         [&region](const base::ScopedFD& fd) {
                           region = base::MemoryMappedFile::Region::kWholeFile;
                           return fd.get();
                         }},
        key_path_iter.second);
    if (file == base::kInvalidPlatformFile) {
      continue;
    }
    file_switch_value_builder.AddEntry(key_path_iter.first, key);
    files_to_register->ShareWithRegion(key, file, region);
    key++;
    DCHECK(key < kContentDynamicDescriptorMax);
  }
  command_line->AppendSwitchASCII(switches::kSharedFiles,
                                  file_switch_value_builder.switch_value());

  return files_to_register;
}

}  // namespace internal
}  // namespace content
