// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper_posix.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/posix_file_descriptor_info_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "services/service_manager/embedder/shared_file_util.h"
#include "services/service_manager/embedder/switches.h"

namespace content {
namespace internal {

namespace {

using RequiredFilesByServiceMap =
    std::map<std::string, std::map<std::string, base::FilePath>>;

RequiredFilesByServiceMap& GetRequiredFilesByServiceMap() {
  static auto* required_files_by_service = new RequiredFilesByServiceMap();
  return *required_files_by_service;
}

std::map<std::string, std::string>& GetServiceNameByProcessTypeMap() {
  static auto* service_name_resolver = new std::map<std::string, std::string>(
      {// The service names are defined in the JSON manifests, so we don't have
       // a constant accessible for them.
       // TODO(jcivelli): remove this map once the service name is accessible
       // from the command line crbug.com/687250
       {switches::kGpuProcess, "content_gpu"},
       {switches::kPpapiPluginProcess, "content_plugin"},
       {switches::kRendererProcess, "content_renderer"},
       {switches::kUtilityProcess, "content_utility"},
       {"ppapi-broker", "ppapi_broker"},
       {"nacl-loader", "nacl_loader"},
       {"nacl-loader-nonsfi", "nacl_loader_nonsfi"}});
  return *service_name_resolver;
}

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
    bool include_service_required_files,
    const std::string& process_type,
    base::CommandLine* command_line) {
  std::unique_ptr<PosixFileDescriptorInfo> files_to_register(
      PosixFileDescriptorInfoImpl::Create());

// Mac shared memory doesn't use file descriptors.
#if !defined(OS_MACOSX)
  int fd = base::FieldTrialList::GetFieldTrialDescriptor();
  DCHECK_NE(fd, -1);
  files_to_register->Share(service_manager::kFieldTrialDescriptor, fd);

  DCHECK(mojo_channel_remote_endpoint.is_valid());
  files_to_register->Share(
      service_manager::kMojoIPCChannel,
      mojo_channel_remote_endpoint.platform_handle().GetFD().get());

  // TODO(jcivelli): remove this "if defined" by making
  // GetAdditionalMappedFilesForChildProcess a no op on Mac.
  GetContentClient()->browser()->GetAdditionalMappedFilesForChildProcess(
      *command_line, child_process_id, files_to_register.get());
#endif

  if (!include_service_required_files)
    return files_to_register;

  // Also include the files specified in the services' manifests.
  auto service_name_iter = GetServiceNameByProcessTypeMap().find(process_type);
  DCHECK(service_name_iter != GetServiceNameByProcessTypeMap().end())
      << "No service found for process type " << process_type;
  const std::string& service_name = service_name_iter->second;
  auto files_iter = GetRequiredFilesByServiceMap().find(service_name);
  if (files_iter != GetRequiredFilesByServiceMap().end()) {
    const std::map<std::string, base::FilePath>& required_files_map =
        files_iter->second;
    base::GlobalDescriptors::Key key = kContentDynamicDescriptorStart;
    service_manager::SharedFileSwitchValueBuilder file_switch_value_builder;
    for (const auto& key_path_iter : required_files_map) {
      base::MemoryMappedFile::Region region;
      base::PlatformFile file =
          OpenFileIfNecessary(key_path_iter.second, &region);
      if (file == base::kInvalidPlatformFile) {
        DLOG(WARNING) << "Ignoring invalid file "
                      << key_path_iter.second.value();
        continue;
      }
      file_switch_value_builder.AddEntry(key_path_iter.first, key);
      files_to_register->ShareWithRegion(key, file, region);
      key++;
      DCHECK(key < kContentDynamicDescriptorMax);
    }
    command_line->AppendSwitchASCII(service_manager::switches::kSharedFiles,
                                    file_switch_value_builder.switch_value());
  }

  return files_to_register;
}

void SetFilesToShareForServicePosix(
    const std::string& service_name,
    std::map<std::string, base::FilePath> required_files) {
  if (required_files.empty())
    return;

  if (!base::StartsWith(service_name, "content_",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    // Not a content child service, ignore.
    return;
  }

  DCHECK(GetRequiredFilesByServiceMap().count(service_name) == 0);
  GetRequiredFilesByServiceMap()[service_name] = std::move(required_files);
}

void ResetFilesToShareForTestingPosix() {
  GetRequiredFilesByServiceMap().clear();
}

}  // namespace internal
}  // namespace content
