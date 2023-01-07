// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_SEARCH_PATH_LIST_H
#define CRAZY_LINKER_SEARCH_PATH_LIST_H

#include <stdint.h>
#include <string.h>

#include "crazy_linker_util.h"  // for String

namespace crazy {

// A simple class to model a list of search paths, and perform
// file system probing with it.
class SearchPathList {
 public:
  SearchPathList() = default;

  // Reset the list, i.e. make it empty.
  void Reset();

  // Reset the list from an environment variable value.
  void ResetFromEnv(const char* var_name);

  // Add one or more paths to the list.
  // |path_list| contains a list of paths separated by columns.
  // |path_list_end| points after the list's last character.
  //
  // NOTE: Adding a zip archive to the list is supported by using an
  // exclamation mark as a delimiter inside a path. For example, the value
  // '/path/to/archive.zip!lib/armeabi-v7a', means looking for libraries
  // inside the zip archive at '/path/to/archive.zip', that are stored
  // as 'lib/armeabi-v7a/<libname>', or even 'lib/armeabi-v7a/crazy.<libname>'.
  // Read the documentation for FindFile() below for more details.
  void AddPaths(const char* path_list, const char* path_list_end);

  // Convenience function that takes a 0-terminated string.
  void AddPaths(const char* path_list) {
    AddPaths(path_list, path_list + ::strlen(path_list));
  }

  // The result of FindFile() below.
  // |path| is the path to the file containing the library, or nullptr on error.
  // |offset| is the byte offset within |path| where the library is located.
  struct Result {
    String path;
    int32_t offset = 0;

    // Returns true iff this instance matches a valid file and offset.
    inline bool IsValid() const { return !path.IsEmpty(); }
  };

  // Try to find a library file named |file_name| by probing the directories
  // added through AddPaths(). This returns a (path, offset) tuple where
  // |path| corresponds to the file path to load the library from, and
  // |offset| to its offset inside the file. This allows loading libraries
  // directly from zip archives, when they are uncompressed and page-aligned
  // within them.
  //
  // In case of failure, the path will be empty, and the offset will be 0.
  //
  // Note that if |file_name| does not contain any directory separator or
  // exclamation name, the corresponding file will be searched in the list
  // of paths added through AddPaths(). Otherwise, this is considered a direct
  // file path and the search list will be ignored.
  //
  // File paths, either given directly by |file_name| or created by prepending
  // search list items to it, can contain an exclamation mark to indicate that
  // the library should be looked into a zip archive. For a concrete example
  // 'path/to/archive.zip!libs/libfoo.so' will look into the zip archive
  // at 'path/to/archive.zip' for a file within it named 'libs/libfoo.so'.
  //
  // This matches the behaviour of the Android system linker, starting with M
  // (i.e. API level 23), but can be used on previous Android releases too.
  //
  // Said libraries must be uncompressed and page-aligned for the linker
  // to later be able to load them properly.
  //
  // NOTE: It is also possible to store library files with a 'crazy.' prefix
  // inside zip archives. In the following example, the function will first
  // look for 'libs/libfoo.so', and if not found, will also look for a file
  // named 'libs/crazy.libfoo.so'.
  //
  // Using such a prefix is useful on Android: such libraries can still be
  // loaded directly from the APK, but will not be extracted by the system
  // at installation into the application data directory (at least before
  // Android M). Note that said libraries should simply be renamed within
  // the zip file (i.e. they should still use the same internal DT_SONAME
  // of 'libfoo.so' for the linker to work properly).
  //
  Result FindFile(const char* file_name) const;

 private:
  String list_;
  String env_list_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_SEARCH_PATH_LIST_H
