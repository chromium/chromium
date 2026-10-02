// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_error.h"

#include <cerrno>
#include <sstream>

#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(TerminalErrorTest, FromSystemErrorIncludesCodeAndDescription) {
  TerminalError error = TerminalError::FromSystemError(
      FROM_HERE, TerminalError::Reason::kPtyError, "posix_openpt", ENOENT);
  EXPECT_EQ(error.reason, TerminalError::Reason::kPtyError);
  EXPECT_EQ(error.system_error_code, ENOENT);
  EXPECT_EQ(error.message,
            base::StrCat({"posix_openpt failed: ",
                          logging::SystemErrorCodeToString(ENOENT)}));
}

TEST(TerminalErrorTest, StreamOperatorIncludesMessageAndLocation) {
  base::Location location = FROM_HERE;
  TerminalError error(location, TerminalError::Reason::kInternalError,
                      "something went wrong");
  EXPECT_FALSE(error.system_error_code.has_value());

  std::ostringstream stream;
  stream << error;
  EXPECT_EQ(stream.str(),
            base::StrCat({"something went wrong [", location.file_name(), ":",
                          base::NumberToString(location.line_number()), "]"}));
}

TEST(TerminalErrorTest, StreamOperatorOmitsMissingLocation) {
  TerminalError error(base::Location(), TerminalError::Reason::kInternalError,
                      "something went wrong");

  std::ostringstream stream;
  stream << error;
  EXPECT_EQ(stream.str(), "something went wrong");
}

}  // namespace remoting
