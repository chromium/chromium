// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "content/common/external_ipc_dumper.h"

namespace {

typedef IPC::ChannelProxy::OutgoingMessageFilter* (*GetFilterFunction)();
typedef void (*SetDumpDirectoryFunction)(const base::FilePath&);

const char kFilterEntryName[] = "GetFilter";
const char kSetDumpDirectoryEntryName[] = "SetDumpDirectory";

#if BUILDFLAG(IS_WIN)
#define IPC_MESSAGE_DUMP_MODULE FILE_PATH_LITERAL("ipc_message_dump.dll")
#else
#define IPC_MESSAGE_DUMP_MODULE FILE_PATH_LITERAL("libipc_message_dump.so")
#endif

}  // namespace

namespace content {

NO_SANITIZE("cfi-icall")
IPC::ChannelProxy::OutgoingMessageFilter* LoadExternalIPCDumper(
    const base::FilePath& dump_directory) {
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_MODULE, &module_path)) {
    LOG(ERROR) << "Unable to get message dump module directory.";
    return NULL;
  }

  base::FilePath library_path = module_path.Append(IPC_MESSAGE_DUMP_MODULE);
  base::NativeLibraryLoadError load_error;
  base::NativeLibrary library =
      base::LoadNativeLibrary(library_path, &load_error);

  if (!library) {
    LOG(ERROR) << load_error.ToString();
    return NULL;
  }

  SetDumpDirectoryFunction set_directory_entry_point =
      reinterpret_cast<SetDumpDirectoryFunction>(
          base::GetFunctionPointerFromNativeLibrary(
              library, kSetDumpDirectoryEntryName));
  if (!set_directory_entry_point) {
    LOG(ERROR) << kSetDumpDirectoryEntryName
               << " not exported by message dump module.";
    return NULL;
  }
  set_directory_entry_point(dump_directory);

  GetFilterFunction filter_entry_point = reinterpret_cast<GetFilterFunction>(
      base::GetFunctionPointerFromNativeLibrary(library, kFilterEntryName));
  if (!filter_entry_point) {
    LOG(ERROR) << kFilterEntryName << " not exported by message dump module.";
    return NULL;
  }

  return filter_entry_point();
}

}  // namespace content
