# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

def CheckForbiddenRegex(change, forbidden_regex, message_type, message):
  problems = []
  for path, change_per_file in change:
    line_num = 1
    for line in change_per_file:
      if forbidden_regex.match(line):
        problems.extend(["  %s:%d" % (path, line_num)])
      line_num += 1
  if not problems:
    return []
  return [message_type(message + ":\n" + "\n".join(problems))]


def CheckChange(input_api, message_type):
  result = []
  shared_source_files = re.compile("^net/third_party/spdy/(core|platform/api)/.*\.(h|cc)$")
  change = [(affected_file.LocalPath(), affected_file.NewContents())
            for affected_file in input_api.AffectedTestableFiles()
            if shared_source_files.match(affected_file.LocalPath())]
  forbidden_regex_list = [
      r"^#include \"net/base/net_export.h\"$",
      r"\bNET_EXPORT\b",
      r"\bNET_EXPORT_PRIVATE\b",
      "^#include <string>$",
      r"\bstd::string\b",
      r"^#include \"base/strings/string_piece.h\"$",
      r"^#include \"net/base/hex_utils.h\"$",
      r"\bbase::StringPiece\b",
      r"\bbase::StringPrintf\b",
      r"\bbase::StringAppendF\b",
      r"\bbase::HexDigitToInt\b",
      r"\bHexDecode\b",
      r"\bHexDump\b",
  ]
  messages = [
      "Include \"net/third_party/spdy/platform/api/spdy_export.h\" "
          "instead of \"net/base/net_export.h\"",
      "Use SPDY_EXPORT instead of NET_EXPORT",
      "Use SPDY_EXPORT_PRIVATE instead of NET_EXPORT_PRIVATE",
      "Include \"net/third_party/spdy/platform/api/spdy_string.h\" instead of <string>",
      "Use SpdyString instead of std::string",
      "Include \"net/third_party/spdy/platform/api/spdy_string_piece.h\" "
          "instead of \"base/strings/string_piece.h\"",
      "Include \"net/third_party/spdy/platform/api/spdy_string_utils.h\" "
          "instead of \"net/base/hex_utils.h\"",
      "Use SpdyStringPiece instead of base::StringPiece",
      "Use SpdyStrCat instead of base::StringPrintf",
      "Use SpdyStrCat instead of base::StringAppendF",
      "Use SpdyHexDigitToInt instead of base::HexDigitToInt",
      "Use SpdyHexDecode instead of HexDecode",
      "Use SpdyHexDump instead of HexDump",
  ]
  for forbidden_regex, message in zip(forbidden_regex_list, messages):
    result.extend(CheckForbiddenRegex(
        change, re.compile(forbidden_regex), message_type, message))
  return result

# Warn before uploading but allow developer to skip warning
# so that CLs can be shared and reviewed before addressing all issues.
def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api.PresubmitPromptWarning)

# Do not allow code with forbidden patterns to be checked in.
def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api.PresubmitError)
