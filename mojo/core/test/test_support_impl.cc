// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/core/test/test_support_impl.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/perf_log.h"

namespace mojo {
namespace core {
namespace test {
namespace {

base::FilePath ResolveSourceRootRelativePath(const char* relative_path) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path)) {
    return base::FilePath();
  }

  for (std::string_view component : base::SplitStringPiece(
           relative_path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (!component.empty())
      path = path.AppendASCII(component);
  }

  return path;
}

}  // namespace

TestSupportImpl::TestSupportImpl() = default;

TestSupportImpl::~TestSupportImpl() = default;

void TestSupportImpl::LogPerfResult(const char* test_name,
                                    const char* sub_test_name,
                                    double value,
                                    const char* units) {
  DCHECK(test_name);
  if (sub_test_name) {
    std::string name = base::StringPrintf("%s/%s", test_name, sub_test_name);
    base::LogPerfResult(name.c_str(), value, units);
  } else {
    base::LogPerfResult(test_name, value, units);
  }
}

FILE* TestSupportImpl::OpenSourceRootRelativeFile(const char* relative_path) {
  return base::OpenFile(ResolveSourceRootRelativePath(relative_path), "rb");
}

char** TestSupportImpl::EnumerateSourceRootRelativeDirectory(
    const char* relative_path) {
  std::vector<std::string> names;
  base::FileEnumerator e(ResolveSourceRootRelativePath(relative_path), false,
                         base::FileEnumerator::FILES);
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next())
    names.push_back(name.BaseName().AsUTF8Unsafe());

  // |names.size() + 1| for null terminator.
  char** rv = static_cast<char**>(calloc(names.size() + 1, sizeof(char*)));
  for (size_t i = 0; i < names.size(); ++i)
    rv[i] = base::strdup(names[i].c_str());
  return rv;
}

}  // namespace test
}  // namespace core
}  // namespace mojo
