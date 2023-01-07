// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_node.h"

#include <string.h>

#include <algorithm>

#include <ppapi/c/pp_errors.h>

FakeNode::FakeNode(const PP_FileInfo& info) : info_(info) {}

FakeNode::FakeNode(const PP_FileInfo& info,
                   const std::vector<uint8_t>& contents)
    : info_(info), contents_(contents) {}

FakeNode::FakeNode(const PP_FileInfo& info, const std::string& contents)
    : info_(info) {
  std::copy(contents.begin(), contents.end(), std::back_inserter(contents_));
}

int32_t FakeNode::Read(int64_t offset, char* buffer, int32_t bytes_to_read) {
  if (offset < 0)
    return PP_ERROR_FAILED;

  bytes_to_read =
      std::max(0, std::min<int32_t>(bytes_to_read, contents_.size() - offset));
  memcpy(buffer, contents_.data() + offset, bytes_to_read);
  return bytes_to_read;
}

int32_t FakeNode::Write(int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write) {
  if (offset < 0)
    return PP_ERROR_FAILED;

  size_t new_size = offset + bytes_to_write;
  if (new_size > contents_.size())
    contents_.resize(new_size);

  memcpy(contents_.data() + offset, buffer, bytes_to_write);
  info_.size = new_size;
  return bytes_to_write;
}

int32_t FakeNode::Append(const char* buffer, int32_t bytes_to_write) {
  return Write(contents_.size(), buffer, bytes_to_write);
}

int32_t FakeNode::SetLength(int64_t length) {
  contents_.resize(length);
  info_.size = length;
  return PP_OK;
}

void FakeNode::GetInfo(PP_FileInfo* out_info) {
  *out_info = info_;
}

bool FakeNode::IsRegular() const {
  return info_.type == PP_FILETYPE_REGULAR;
}

bool FakeNode::IsDirectory() const {
  return info_.type == PP_FILETYPE_DIRECTORY;
}
