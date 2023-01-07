// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/path.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "sdk_util/string_util.h"

namespace nacl_io {

Path::Path() : len_(0) {
  path_[0] = 0;
}

Path::Path(const Path& path) {
  len_ = path.len_;
  strcpy(path_, path.path_);
}

Path::Path(const std::string& path) {
  Set(path);
}

bool Path::IsAbsolute() const {
  return path_[0] == '/';
}

std::string Path::Part(size_t index) const {
  if (IsAbsolute() && index == 0) {
    return std::string("/");
  }

  const char* start = &path_[0];
  size_t slashes = 0;
  const char* p;
  for (p = &path_[0]; *p; p++) {
    if (*p == '/') {
      if (++slashes == index + 1)
        break;

      start = p + 1;
    }
  }

  return std::string(start, p - start);
}

size_t Path::Size() const {
  if (len_ == 0)
    return 0;

  const char* p = &path_[0];
  if (len_ == 1 && *p == '/') {
    return 1;
  }

  size_t count = 1;
  for (; *p; p++) {
    if (*p == '/')
      count++;
  }
  return count;
}

bool Path::IsRoot() const {
  return strcmp(path_, "/") == 0;
}

Path& Path::MakeRelative() {
  if (IsAbsolute()) {
    memmove(&path_[0], &path_[1], PATH_MAX - 1);
    len_--;
  }
  return *this;
}

Path& Path::Append(const Path& path) {
  // Appending an absolute path effectivly sets the path, ignoring
  // the current contents.
  if (path.IsAbsolute()) {
    strcpy(path_, path.path_);
  } else {
    strncat(path_, "/", PATH_MAX - len_ - 1);
    len_++;
    strncat(path_, path.path_, PATH_MAX - len_ - 1);
    len_ += path.len_;

    if (len_ >= PATH_MAX - 1) {
      len_ = PATH_MAX - 1;
    }
  }

  Normalize();
  return *this;
}

Path& Path::Append(const std::string& path) {
  return Append(Path(path));
}

Path& Path::Set(const std::string& path) {
  strncpy(path_, path.c_str(), PATH_MAX - 1);
  path_[PATH_MAX - 1] = 0;
  len_ = path.length();
  if (len_ > PATH_MAX - 1)
    len_ = PATH_MAX - 1;
  Normalize();
  return *this;
}

Path Path::Parent() const {
  const char* last_slash = strrchr(path_, '/');
  if (last_slash) {
    Path out;
    if (last_slash == &path_[0]) {
      out.len_ = 1;
      strcpy(out.path_, "/");
    } else {
      out.len_ = last_slash - &path_[0];
      strncpy(out.path_, path_, out.len_);
      out.path_[out.len_] = 0;
    }

    return out;
  }

  return Path(*this);
}

std::string Path::Basename() const {
  if (IsRoot())
    return std::string(path_);

  const char* last_slash = strrchr(path_, '/');
  if (last_slash)
    return std::string(last_slash + 1, path_ + len_ - (last_slash + 1));

  return std::string(path_);
}

std::string Path::Join() const {
  return std::string(path_);
}

std::string Path::Range(size_t start, size_t end) const {
  assert(start <= end);

  const char* pstart = &path_[0];
  const char* pend = &path_[len_];

  if (IsAbsolute() && start == 0 && end == 1)
    return std::string("/");

  size_t slashes = 0;
  for (const char* p = &path_[0]; *p; p++) {
    if (*p == '/') {
      ++slashes;
      if (slashes == start)
        pstart = p + 1;

      if (slashes == end) {
        pend = p;
        break;
      }
    }
  }

  if (slashes < start || pstart > pend)
    return std::string();

  return std::string(pstart, pend - pstart);
}

void Path::Normalize() {
  char* outp = &path_[0];
  const char* start = outp;
  const char* part_start = start;
  const char* next_slash;
  bool is_absolute = false;

  if (IsAbsolute()) {
    // Absolute path. Append the slash, then continue the algorithm as if the
    // path were relative.
    start++;
    outp++;
    part_start++;
    is_absolute = true;
  }

  do {
    next_slash = strchr(part_start, '/');
    const char* part_end = next_slash;
    if (!part_end)
      part_end = part_start + strlen(part_start);

    size_t part_len = part_end - part_start;

    bool should_append = true;
    if (part_len == 0) {
      // Don't append if the part is empty.
      should_append = false;
    } else if (part_len == 1 && part_start[0] == '.') {
      // Don't append "."
      should_append = false;
    } else if (part_len == 2 && part_start[0] == '.' && part_start[1] == '.') {
      // If part is "..", only append if the output is empty or already has
      // ".." at the end.
      if (outp == start ||
          (outp - start >= 2 && outp[-1] == '.' && outp[-2] == '.')) {
        should_append = !is_absolute;
      } else {
        should_append = false;
        // Move outp backward to the one past the previous slash, or to the
        // beginning of the string. Unless outp == start, outp[-1] is a '/'.
        if (outp > start)
          --outp;
        while (outp > start && outp[0] != '/')
          --outp;
      }
    }

    if (should_append) {
      // Append [part_start, part_end) to outp.
      if (outp != start) {
        // Append slash to separate from previous path.
        *outp++ = '/';
      }

      // Only need to copy bytes when the pointers are different.
      if (outp != part_start) {
        memmove(outp, part_start, part_len);
      }

      outp += part_len;
    }

    part_start = next_slash + 1;
  } while (next_slash);

  // Return '.' instead of an empty path.
  if (outp == start && !is_absolute) {
    *outp++ = '.';
  }

  *outp = 0;
  len_ = outp - &path_[0];
}

Path& Path::operator=(const Path& p) {
  len_ = p.len_;
  strcpy(path_, p.path_);
  return *this;
}

Path& Path::operator=(const std::string& p) {
  return Set(p);
}

bool Path::operator==(const Path& other) {
  return len_ == other.len_ && strncmp(path_, other.path_, len_) == 0;
}

bool Path::operator!=(const Path& other) {
  return !operator==(other);
}

}  // namespace nacl_io
