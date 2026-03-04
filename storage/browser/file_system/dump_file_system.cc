// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A tool to dump HTML5 filesystem from CUI.
//
// Usage:
//
// ./out/Release/dump_file_system [options] <filesystem dir> [origin]...
//
// If no origin is specified, this dumps all origins in the profile dir.
//
// Available options:
//
// -t : dumps temporary files instead of persistent.
// -s : dumps syncable files instead of persistent.
// -l : more information will be displayed.
//
// The format of -l option is:
//
// === ORIGIN origin_name origin_dir ===
// file_name file_id file_size file_content_path
// ...
//
// where file_name has a trailing slash, file_size is the number of
// children, and file_content_path is empty if the file is a directory.
//

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/stack.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/sandbox_directory_database.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/file_system/sandbox_origin_database.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"

namespace {

bool g_opt_long;
const base::FilePath::CharType* g_opt_fs_type = FILE_PATH_LITERAL("p");

void ShowMessageAndExit(const std::string& msg) {
  fprintf(stderr, "%s\n", msg.c_str());
  exit(EXIT_FAILURE);
}

void ShowUsageAndExit(const std::string& arg0) {
  ShowMessageAndExit("Usage: " + arg0 +
                     " [-l] [-t] [-s] <filesystem dir> [origin]...");
}

}  // namespace

namespace storage {

static void DumpDirectoryTree(const std::string& origin_name,
                              base::FilePath origin_dir) {
  origin_dir = origin_dir.Append(g_opt_fs_type);

  printf("=== ORIGIN %s %s ===\n",
         origin_name.c_str(), FilePathToString(origin_dir).c_str());

  if (!base::DirectoryExists(origin_dir))
    return;

  SandboxDirectoryDatabase directory_db(origin_dir, nullptr);
  SandboxDirectoryDatabase::FileId root_id;
  if (!directory_db.GetFileWithPath(StringToFilePath("/"), &root_id))
    return;

  base::stack<std::pair<SandboxDirectoryDatabase::FileId, std::string>> paths;
  paths.push(std::make_pair(root_id, ""));
  while (!paths.empty()) {
    SandboxDirectoryDatabase::FileId id = paths.top().first;
    const std::string dirname = paths.top().second;
    paths.pop();

    SandboxDirectoryDatabase::FileInfo info;
    if (!directory_db.GetFileInfo(id, &info)) {
      ShowMessageAndExit(
          base::StringPrintf("GetFileInfo failed for %" PRId64, id));
    }

    const std::string name =
        dirname + "/" + FilePathToString(base::FilePath(info.name));
    std::vector<SandboxDirectoryDatabase::FileId> children;
    if (info.is_directory()) {
      if (!directory_db.ListChildren(id, &children)) {
        ShowMessageAndExit(base::StringPrintf(
            "ListChildren failed for %" PRFilePath " (%" PRId64 ")",
            info.name.c_str(), id));
      }

      for (size_t j = children.size(); j; j--)
        paths.push(make_pair(children[j - 1], name));
    }

    // Skip the leading slash.
    const std::string display_name =
        name.substr(1u) + (info.is_directory() ? "/" : "");
    if (g_opt_long) {
      int64_t size;
      if (info.is_directory()) {
        size = static_cast<int64_t>(children.size());
      } else {
        size = base::GetFileSize(origin_dir.Append(info.data_path)).value_or(0);
      }
      // TODO(hamaji): Modification time?
      printf("%s %" PRId64 " %" PRId64 " %s\n", display_name.c_str(), id, size,
             FilePathToString(info.data_path).c_str());
    } else {
      printf("%s\n", display_name.c_str());
    }
  }
}

static base::FilePath GetOriginDir(const base::FilePath& file_system_dir,
                                   const std::string& origin_name) {
  SandboxOriginDatabase origin_db(file_system_dir, nullptr);
  base::FilePath origin_dir;
  if (!origin_db.HasOriginPath(origin_name)) {
    ShowMessageAndExit("Origin " + origin_name + " is not in " +
                       FilePathToString(file_system_dir));
  }

  if (!origin_db.GetPathForOrigin(origin_name, &origin_dir)) {
    ShowMessageAndExit("Failed to get path of origin " + origin_name + " in " +
                       FilePathToString(file_system_dir));
  }

  return origin_dir;
}

static void DumpOrigin(const base::FilePath& file_system_dir,
                       const std::string& origin_name) {
  base::FilePath origin_dir = GetOriginDir(file_system_dir, origin_name);
  DumpDirectoryTree(origin_name, file_system_dir.Append(origin_dir));
}

static void DumpFileSystem(const base::FilePath& file_system_dir) {
  SandboxOriginDatabase origin_db(file_system_dir, nullptr);
  std::vector<SandboxOriginDatabase::OriginRecord> origins;
  origin_db.ListAllOrigins(&origins);
  for (size_t i = 0; i < origins.size(); i++) {
    const SandboxOriginDatabase::OriginRecord& origin = origins[i];
    DumpDirectoryTree(origin.origin, file_system_dir.Append(origin.path));
    puts("");
  }
}

}  // namespace storage

int main(int argc, char* argv[]) {
  // SAFETY: argc and argv are provided by the OS.
  base::span<char*> args =
      UNSAFE_TODO(base::span(argv, static_cast<size_t>(argc)));
  const char* arg0 = args[0];
  size_t arg_index = 1u;
  while (arg_index < args.size()) {
    const std::string_view arg = args[arg_index];
    if (arg == "-l") {
      g_opt_long = true;
      ++arg_index;
    } else if (arg == "-t") {
      g_opt_fs_type = FILE_PATH_LITERAL("t");
      ++arg_index;
    } else if (arg == "-s") {
      g_opt_fs_type = FILE_PATH_LITERAL("s");
      ++arg_index;
    } else {
      break;
    }
  }

  if (arg_index >= args.size()) {
    ShowUsageAndExit(arg0);
  }

  const base::FilePath file_system_dir =
      storage::StringToFilePath(args[arg_index]);
  if (!base::DirectoryExists(file_system_dir)) {
    ShowMessageAndExit(storage::FilePathToString(file_system_dir) +
                       " is not a filesystem directory");
  }

  if (arg_index + 1u >= args.size()) {
    storage::DumpFileSystem(file_system_dir);
  } else {
    for (size_t i = arg_index + 1u; i < args.size(); ++i) {
      storage::DumpOrigin(file_system_dir, args[i]);
    }
  }
  return 0;
}
