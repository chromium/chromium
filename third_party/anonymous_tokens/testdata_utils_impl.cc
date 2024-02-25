// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "testdata_utils_impl.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"

namespace {

const base::FilePath::CharType kTestdataPath[] = FILE_PATH_LITERAL(
    "third_party/anonymous_tokens/src/anonymous_tokens/testdata/");

}

std::string GetTestdataPathImpl() {
  base::FilePath src_root;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root);
  }
  return src_root.Append(kTestdataPath).MaybeAsASCII();
}
