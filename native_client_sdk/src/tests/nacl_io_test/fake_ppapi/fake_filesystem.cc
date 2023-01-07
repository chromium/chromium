// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_filesystem.h"

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

#include "fake_ppapi/fake_node.h"

FakeFilesystem::FakeFilesystem() : filesystem_type_(PP_FILESYSTEMTYPE_INVALID) {
  Clear();
}

FakeFilesystem::FakeFilesystem(PP_FileSystemType type)
    : filesystem_type_(type) {
  Clear();
}

FakeFilesystem::FakeFilesystem(const FakeFilesystem& filesystem,
                               PP_FileSystemType type)
    : node_map_(filesystem.node_map_), filesystem_type_(type) {}

void FakeFilesystem::Clear() {
  node_map_.clear();
  // Always have a root node.
  AddDirectory("/", NULL);
}

bool FakeFilesystem::AddEmptyFile(const Path& path, FakeNode** out_node) {
  return AddFile(path, std::vector<uint8_t>(), out_node);
}

bool FakeFilesystem::AddFile(const Path& path,
                             const std::string& contents,
                             FakeNode** out_node) {
  std::vector<uint8_t> data;
  std::copy(contents.begin(), contents.end(), std::back_inserter(data));
  return AddFile(path, data, out_node);
}

bool FakeFilesystem::AddFile(const Path& path,
                             const std::vector<uint8_t>& contents,
                             FakeNode** out_node) {
  NodeMap::iterator iter = node_map_.find(path);
  if (iter != node_map_.end()) {
    if (out_node)
      *out_node = NULL;
    return false;
  }

  PP_FileInfo info;
  info.size = contents.size();
  info.type = PP_FILETYPE_REGULAR;
  info.system_type = filesystem_type_;
  info.creation_time = 0;
  info.last_access_time = 0;
  info.last_modified_time = 0;

  FakeNode node(info, contents);
  std::pair<NodeMap::iterator, bool> result =
      node_map_.insert(NodeMap::value_type(path, node));

  EXPECT_EQ(true, result.second);
  if (out_node)
    *out_node = &result.first->second;
  return true;
}

bool FakeFilesystem::AddDirectory(const Path& path, FakeNode** out_node) {
  NodeMap::iterator iter = node_map_.find(path);
  if (iter != node_map_.end()) {
    if (out_node)
      *out_node = NULL;
    return false;
  }

  PP_FileInfo info;
  info.size = 0;
  info.type = PP_FILETYPE_DIRECTORY;
  info.system_type = filesystem_type_;
  info.creation_time = 0;
  info.last_access_time = 0;
  info.last_modified_time = 0;

  FakeNode node(info);
  std::pair<NodeMap::iterator, bool> result =
      node_map_.insert(NodeMap::value_type(path, node));

  EXPECT_EQ(true, result.second);
  if (out_node)
    *out_node = &result.first->second;
  return true;
}

bool FakeFilesystem::RemoveNode(const Path& path) {
  return node_map_.erase(path) >= 1;
}

FakeNode* FakeFilesystem::GetNode(const Path& path) {
  NodeMap::iterator iter = node_map_.find(path);
  if (iter == node_map_.end())
    return NULL;
  return &iter->second;
}

bool FakeFilesystem::GetDirectoryEntries(
    const Path& path,
    DirectoryEntries* out_dir_entries) const {
  out_dir_entries->clear();

  NodeMap::const_iterator iter = node_map_.find(path);
  if (iter == node_map_.end())
    return false;

  const FakeNode& dir_node = iter->second;
  if (!dir_node.IsDirectory())
    return false;

  for (NodeMap::const_iterator iter = node_map_.begin();
       iter != node_map_.end(); ++iter) {
    const Path& node_path = iter->first;
    if (node_path.find(path) == std::string::npos)
      continue;

    // A node is not a child of itself.
    if (&iter->second == &dir_node)
      continue;

    // Only consider children, not descendants. If we find a forward slash, then
    // the node must be in a subdirectory.
    if (node_path.find('/', path.size() + 1) != std::string::npos)
      continue;

    // The directory entry names do not include the path.
    Path entry_path = node_path;
    size_t last_slash = node_path.rfind('/');
    if (last_slash != std::string::npos)
      entry_path.erase(0, last_slash + 1);

    DirectoryEntry entry;
    entry.path = entry_path;
    entry.node = &iter->second;
    out_dir_entries->push_back(entry);
  }

  return true;
}

// static
FakeFilesystem::Path FakeFilesystem::GetParentPath(const Path& path) {
  size_t last_slash = path.rfind('/');
  if (last_slash == 0)
    return "/";

  EXPECT_EQ(std::string::npos, last_slash);
  return path.substr(0, last_slash);
}
