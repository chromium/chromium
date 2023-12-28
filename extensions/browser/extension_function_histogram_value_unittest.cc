// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_histogram_value.h"

#include <map>
#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ExtensionFunctionHistogramValueTest, CheckEnums) {
  std::optional<base::HistogramEnumEntryMap> enums = base::ReadEnumFromEnumsXml(
      "ExtensionFunctions", /*subdirectory=*/"extensions");
  ASSERT_TRUE(enums);
  // The number of enums in the histogram entry should be equal to the number of
  // enums in the C++ file.
  EXPECT_EQ(enums->size(), functions::ENUM_BOUNDARY);

  base::FilePath src_root;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root));
  base::FilePath function_histogram_value =
      src_root.AppendASCII("extensions")
          .AppendASCII("browser")
          .AppendASCII("extension_function_histogram_value.h");
  ASSERT_TRUE(base::PathExists(function_histogram_value));

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(function_histogram_value, &file_contents));

  for (const auto& entry : *enums) {
    // Check that the C++ file has a definition equal to the histogram file.
    // For now, we do this in a simple, but reasonably effective, manner:
    // expecting to find the string " ENTRY = <value>" somewhere in the file.
    // Notes:
    // - We prepend a space here (" ENTRY =" instead of "ENTRY =") so that
    //   it also enforces updating the entry if you rename it to
    //   `DELETED_FOO_METHOD`.
    // - This doesn't work with multi-line declarations in the enum file. It's
    //   not (yet) worth making it smart enough to deal with that.
    std::string expected_string =
        base::StringPrintf(" %s = %d", entry.second.c_str(), entry.first);
    EXPECT_TRUE(base::Contains(file_contents, expected_string))
        << "Failed to find entry " << entry.second << " with value "
        << entry.first;
  }
}

}  // namespace extensions
