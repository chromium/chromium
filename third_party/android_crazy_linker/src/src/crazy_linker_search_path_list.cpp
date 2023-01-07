// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_search_path_list.h"

#include "crazy_linker.h"
#include "crazy_linker_debug.h"
#include "crazy_linker_system.h"
#include "crazy_linker_zip.h"

#include <utility>

#include <string.h>

namespace crazy {

namespace {

// Helper class used to parse over the items of two column-separated lists.
// Usage is the following:
//  1) Create new instance, passing the first and second list as parameters.
//  2) Call NextItem() in a loop until it returns false. Each call will
//     return the current item.
//
// Items of the first list are returned in order before items of the second
// list.
class MultiListParser {
 public:
  // Constructor.
  MultiListParser(const String& a_list, const String& b_list)
      : p_(a_list.cbegin()), end_(a_list.cend()), b_list_(b_list) {}

  // Grab next item. On success return true and sets |*result|. On end of list,
  // just return false.
  bool NextItem(String* result) {
    for (;;) {
      if (p_ == end_) {
        if (p_ != b_list_.cend()) {
          p_ = b_list_.cbegin();
          end_ = b_list_.cend();
          continue;
        }
        return false;
      }
      // compute current list item, and next item start at the same time.
      const char* item = p_;
      const char* item_end = item;
      while (item_end < end_ && item_end[0] != ':')
        item_end++;

      p_ = item_end + (item_end < end_);

      if (item_end > item) {  // Skip over empty entries.
        result->Assign(item, item_end - item);
        return true;
      }
    }
  }

 private:
  const char* p_;
  const char* end_;
  const String& b_list_;
};

// Look into zip archive at |zip_path| for a file named |file_name|.
// As a special convenience trick, this will also try to find a file with
// the same name with a 'crazy.' prefix. In other words, when looking for
// 'lib/libfoo.so', this will first look for 'lib/libfoo.so', then for
// 'lib/crazy.libfoo.so'. This allows storing uncompressed libraries inside
// Android APKs while preventing the system from extracting them at installation
// time (which will always happen before Android M). Note that these files
// should just be renamed, i.e. their internal soname should still be
// 'libfoo.so'.
//
// On success, return offset within zip archive, or CRAZY_OFFSET_FAILED
// if the library is not found.
int32_t FindLibFileInZipArchive(const char* zip_path, const char* file_name) {
  // First attempt is direct lookup.
  int32_t offset = FindStartOffsetOfFileInZipFile(zip_path, file_name);
  if (offset != CRAZY_OFFSET_FAILED) {
    LOG("  FOUND_IN_ZIP %s!%s @ 0x%x", zip_path, file_name, offset);
    return offset;
  }

  // Second attempt adding a crazy. prefix to the library name.
  String crazy_name;
  const char* pos = ::strrchr(file_name, '/');
  if (pos) {
    crazy_name.Assign(file_name, (pos + 1 - file_name));
    file_name = pos + 1;
  }
  crazy_name.Append("crazy.");
  crazy_name.Append(file_name);

  offset = FindStartOffsetOfFileInZipFile(zip_path, crazy_name.c_str());
  if (offset != CRAZY_OFFSET_FAILED) {
    LOG("  FOUND IN ZIP %s!%s @ 0x%x", zip_path, crazy_name.c_str(), offset);
  }
  return offset;
}

// Try to find the library file pointed by |path|.
// If |path| contains an exclamation mark, this is interpreted as a separator
// between a zip archive file path, and a file contained inside it.
// Also supports crazy. prefix for storing renamed libraries inside the zip
// archive (see comment for FindLibFileInZipArchive).
SearchPathList::Result FindLibFile(const char* path) {
  // An exclamation mark in the file name indicates that one should look
  // inside a zip archive. This is supported by the platform, see the
  // "Opening shared libraries directly from an APK" in the following article:
  // https://github.com/aosp-mirror/platform_bionic/blob/master/android-changes-for-ndk-developers.md
  const char* bang = ::strchr(path, '!');
  if (bang) {
    if (bang == path || bang[1] == '\0') {
      // An initial or final '!' is always an error.
      LOG("  INVALID_ZIP_PATH %s", path);
    } else {
      String zip_path = MakeAbsolutePathFrom(path, bang - path);
      const char* file_name = bang + 1;
      int32_t offset = FindLibFileInZipArchive(zip_path.c_str(), bang + 1);
      if (offset != CRAZY_OFFSET_FAILED) {
        return {std::move(zip_path), offset};
      }
    }
  } else {
    // Regular file path.
    String file_path = MakeAbsolutePathFrom(path);
    if (PathIsFile(file_path.c_str())) {
      LOG("  FOUND FILE %s", file_path.c_str());
      return {std::move(file_path), 0};
    }
  }

  LOG("  skip %s", path);
  return {};
}

}  // namespace

void SearchPathList::Reset() {
  list_.Resize(0);
  env_list_.Resize(0);
}

void SearchPathList::ResetFromEnv(const char* var_name) {
  Reset();
  const char* env = GetEnv(var_name);
  if (env && *env)
    env_list_ = env;
}

void SearchPathList::AddPaths(const char* list, const char* list_end) {
  // Append a column to the current list, if necessary
  if (list_.size() > 0 && list_[list_.size() - 1] != ':')
    list_ += ':';
  list_.Append(list, list_end - list);
}

SearchPathList::Result SearchPathList::FindFile(const char* file_name) const {
  LOG("Looking for %s", file_name);

  if (::strchr(file_name, '/') != nullptr ||
      ::strchr(file_name, '!') != nullptr) {
    // This is an absolute or relative file path, so ignore the search list.
    return FindLibFile(file_name);
  }

  // Build full list by appending the env_list_ after the regular one.
  MultiListParser parser(list_, env_list_);
  String file_path;
  Result result;
  while (parser.NextItem(&file_path)) {
    // Add trailing directory separator if needed.
    if (file_path[file_path.size() - 1] != '/')
      file_path += '/';

    file_path += file_name;

    result = FindLibFile(file_path.c_str());
    if (result.IsValid()) {
      return result;
    }
    LOG("  SKIPPED %s", file_path.c_str());
  }

  LOG("  MISSING %s", file_name);
  return result;
}

}  // namespace crazy
