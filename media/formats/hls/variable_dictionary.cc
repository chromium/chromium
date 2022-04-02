// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/variable_dictionary.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/types.h"

namespace media::hls {

namespace {

struct GetNextVariableResult {
  // The portion of the string prior to the variable
  SourceString head;

  // The variable name and the portion of the string following it, if one was
  // found.
  absl::optional<std::pair<types::VariableName, SourceString>> tail;
};

GetNextVariableResult GetNextVariable(const SourceString input) {
  // Iterate through occurrences of "{$" in the string.
  for (size_t ref_start = input.Str().find("{$");
       ref_start != base::StringPiece::npos;
       ref_start = input.Str().find("{$", ref_start + 2)) {
    auto remaining_input = input;

    // Extract the substring prior to the variable reference
    const auto head = remaining_input.Consume(ref_start);
    remaining_input.Consume(2);

    // Find the end of the variable reference sequence. If this fails there will
    // be no more valid variable references.
    const auto ref_end = remaining_input.Str().find_first_of('}');
    if (ref_end == base::StringPiece::npos) {
      break;
    }

    // Validate the variable name. If this fails, ignore this sequence and keep
    // searching.
    auto var_name_result =
        types::VariableName::Parse(remaining_input.Consume(ref_end));
    remaining_input.Consume(1);
    if (var_name_result.has_error()) {
      continue;
    }
    auto var_name = std::move(var_name_result).value();

    return GetNextVariableResult{
        .head = head, .tail = std::make_pair(var_name, remaining_input)};
  }

  return GetNextVariableResult{.head = input, .tail = absl::nullopt};
}

}  // namespace

VariableDictionary::VariableDictionary() = default;

VariableDictionary::~VariableDictionary() = default;

VariableDictionary::VariableDictionary(VariableDictionary&&) = default;

VariableDictionary& VariableDictionary::operator=(VariableDictionary&&) =
    default;

absl::optional<base::StringPiece> VariableDictionary::Find(
    types::VariableName name) const& {
  auto iter = entries_.find(name.GetName());
  if (iter == entries_.end()) {
    return absl::nullopt;
  }

  return iter->second;
}

bool VariableDictionary::Insert(types::VariableName name, std::string value) {
  return entries_.try_emplace(std::move(name).GetName(), std::move(value))
      .second;
}

ParseStatus::Or<base::StringPiece> VariableDictionary::Resolve(
    SourceString input,
    SubstitutionBuffer& buffer) const {
  // Get the first variable reference. If this fails, there were no references
  // and we don't need to allocate anything.
  auto next_var = GetNextVariable(input);
  if (!next_var.tail) {
    return next_var.head.Str();
  }

  buffer.buf_.clear();

  while (true) {
    // Append the substring leading to the variable, and abort if there was no
    // variable reference
    buffer.buf_.append(next_var.head.Str().data(), next_var.head.Str().size());
    if (!next_var.tail) {
      break;
    }

    // Look up the variable value
    auto value = Find(next_var.tail->first);
    if (!value) {
      // TODO(crbug.com/1311111): Create a more structured way of serializing
      return ParseStatus(ParseStatusCode::kVariableUndefined)
          .WithData("key", next_var.tail->first.GetName());
    }
    buffer.buf_.append(value->data(), value->size());

    next_var = GetNextVariable(next_var.tail->second);
  }

  return base::StringPiece{buffer.buf_};
}

}  // namespace media::hls
