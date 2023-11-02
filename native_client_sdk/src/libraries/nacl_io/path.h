// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PATH_H_
#define LIBRARIES_NACL_IO_PATH_H_

#include <limits.h>

#include <string>
#include <vector>

#include "sdk_util/macros.h"

namespace nacl_io {

class Path {
 public:
  Path();
  Path(const Path& path);
  explicit Path(const std::string& path);

  // Return true of the first path item is '/'.
  bool IsAbsolute() const;

  // Return true if this is the root path (i.e. it has no parent)
  bool IsRoot() const;

  // Return a part of the path
  std::string Part(size_t index) const;

  // Return the number of path parts
  size_t Size() const;

  // Update the path.
  Path& Append(const Path& path);
  Path& Append(const std::string& path);
  Path& Set(const std::string& path);
  Path& MakeRelative();

  // Return the parent path.
  Path Parent() const;
  std::string Basename() const;

  std::string Join() const;
  std::string Range(size_t start, size_t end) const;

  // Operator versions
  Path& operator=(const Path& p);
  Path& operator=(const std::string& str);
  bool operator==(const Path& other);
  bool operator!=(const Path& other);

 private:
  // Collapse the string list removing extraneous '.', '..' path components
  void Normalize();

  size_t len_;
  char path_[PATH_MAX];
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PATH_H_
