// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/error_utils.h"

#include <initializer_list>

#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace extensions {

namespace {

std::string FormatErrorMessageInternal(
    base::StringPiece format,
    std::initializer_list<base::StringPiece> args) {
  std::string format_str = format.as_string();
  base::StringTokenizer tokenizer(format_str, "*");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);

  std::vector<base::StringPiece> result_pieces;
  auto* args_it = args.begin();
  while (tokenizer.GetNext()) {
    if (!tokenizer.token_is_delim()) {
      result_pieces.push_back(tokenizer.token_piece());
      continue;
    }

    CHECK_NE(args_it, args.end())
        << "More placeholders (*) than substitutions.";

    // Substitute the argument.
    result_pieces.push_back(*args_it);
    args_it++;
  }

  // Not all substitutions were consumed.
  CHECK_EQ(args_it, args.end()) << "Fewer placeholders (*) than substitutions.";

  return base::JoinString(result_pieces, "" /* separator */);
}

}  // namespace

std::string ErrorUtils::FormatErrorMessage(base::StringPiece format,
                                           base::StringPiece s1) {
  return FormatErrorMessageInternal(format, {s1});
}

std::string ErrorUtils::FormatErrorMessage(base::StringPiece format,
                                           base::StringPiece s1,
                                           base::StringPiece s2) {
  return FormatErrorMessageInternal(format, {s1, s2});
}

std::string ErrorUtils::FormatErrorMessage(base::StringPiece format,
                                           base::StringPiece s1,
                                           base::StringPiece s2,
                                           base::StringPiece s3) {
  return FormatErrorMessageInternal(format, {s1, s2, s3});
}

base::string16 ErrorUtils::FormatErrorMessageUTF16(base::StringPiece format,
                                                   base::StringPiece s1) {
  return base::UTF8ToUTF16(FormatErrorMessageInternal(format, {s1}));
}

base::string16 ErrorUtils::FormatErrorMessageUTF16(base::StringPiece format,
                                                   base::StringPiece s1,
                                                   base::StringPiece s2) {
  return base::UTF8ToUTF16(FormatErrorMessageInternal(format, {s1, s2}));
}

base::string16 ErrorUtils::FormatErrorMessageUTF16(base::StringPiece format,
                                                   base::StringPiece s1,
                                                   base::StringPiece s2,
                                                   base::StringPiece s3) {
  return base::UTF8ToUTF16(FormatErrorMessageInternal(format, {s1, s2, s3}));
}

}  // namespace extensions
