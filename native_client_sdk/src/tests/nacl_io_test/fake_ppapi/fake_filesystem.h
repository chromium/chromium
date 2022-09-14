// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILESYSTEM_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILESYSTEM_H_

#include <map>
#include <string>
#include <vector>

#include "ppapi/c/pp_file_info.h"

#include "fake_ppapi/fake_node.h"

class FakeFilesystem {
 public:
  typedef std::string Path;

  struct DirectoryEntry {
    Path path;
    const FakeNode* node;
  };
  typedef std::vector<DirectoryEntry> DirectoryEntries;

  FakeFilesystem();
  explicit FakeFilesystem(PP_FileSystemType type);
  FakeFilesystem(const FakeFilesystem& filesystem, PP_FileSystemType type);

  void Clear();
  bool AddEmptyFile(const Path& path, FakeNode** out_node);
  bool AddFile(const Path& path,
               const std::string& contents,
               FakeNode** out_node);
  bool AddFile(const Path& path,
               const std::vector<uint8_t>& contents,
               FakeNode** out_node);
  bool AddDirectory(const Path& path, FakeNode** out_node);
  bool RemoveNode(const Path& path);

  FakeNode* GetNode(const Path& path);
  bool GetDirectoryEntries(const Path& path,
                           DirectoryEntries* out_dir_entries) const;
  PP_FileSystemType filesystem_type() const { return filesystem_type_; }
  static Path GetParentPath(const Path& path);

 private:
  typedef std::map<Path, FakeNode> NodeMap;
  NodeMap node_map_;
  PP_FileSystemType filesystem_type_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_FILESYSTEM_H_
