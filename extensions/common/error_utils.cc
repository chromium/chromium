// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/error_utils.h"

#include <initializer_list>
#include <ostream>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace extensions {

namespace {

std::string FormatErrorMessageInternal(
    base::StringPiece format,
    base::span<const base::StringPiece> args) {
  std::string format_str(format);
  base::StringTokenizer tokenizer(format_str, "*");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);

  std::vector<base::StringPiece> result_pieces;
  auto argument = args.begin();
  while (tokenizer.GetNext()) {
    if (!tokenizer.token_is_delim()) {
      result_pieces.push_back(tokenizer.token_piece());
      continue;
    }

    CHECK(argument != args.end())
        << "More placeholders (*) than substitutions.";

    // Substitute the argument.
    result_pieces.push_back(*argument);
    std::advance(argument, 1);
  }

  // Not all substitutions were consumed.
  CHECK(argument == args.end()) << "Fewer placeholders (*) than substitutions.";

  return base::JoinString(result_pieces, "" /* separator */);
}

}  // namespace

std::string ErrorUtils::FormatErrorMessage(
    base::StringPiece format,
    base::span<const base::StringPiece> args) {
  return FormatErrorMessageInternal(format, args);
}

std::u16string ErrorUtils::FormatErrorMessageUTF16(
    base::StringPiece format,
    base::span<const base::StringPiece> args) {
  return base::UTF8ToUTF16(FormatErrorMessageInternal(format, args));
}

}  // namespace extensions
