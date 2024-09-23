// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/variable_dictionary.h"

#include <string_view>

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
  std::optional<std::pair<types::VariableName, SourceString>> tail;
};

GetNextVariableResult GetNextVariable(const SourceString input) {
  // Iterate through occurrences of "{$" in the string.
  for (size_t ref_start = input.Str().find("{$");
       ref_start != std::string_view::npos;
       ref_start = input.Str().find("{$", ref_start + 2)) {
    auto remaining_input = input;

    // Extract the substring prior to the variable reference
    const auto head = remaining_input.Consume(ref_start);
    remaining_input.Consume(2);

    // Find the end of the variable reference sequence. If this fails there will
    // be no more valid variable references.
    const auto ref_end = remaining_input.Str().find_first_of('}');
    if (ref_end == std::string_view::npos) {
      break;
    }

    // Validate the variable name. If this fails, ignore this sequence and keep
    // searching.
    auto var_name_result =
        types::VariableName::Parse(remaining_input.Consume(ref_end));
    remaining_input.Consume(1);
    if (!var_name_result.has_value()) {
      continue;
    }
    auto var_name = std::move(var_name_result).value();

    return GetNextVariableResult{
        .head = head, .tail = std::make_pair(var_name, remaining_input)};
  }

  return GetNextVariableResult{.head = input, .tail = std::nullopt};
}

}  // namespace

VariableDictionary::SubstitutionBuffer::SubstitutionBuffer() = default;

VariableDictionary::SubstitutionBuffer::~SubstitutionBuffer() = default;

VariableDictionary::VariableDictionary() = default;

VariableDictionary::~VariableDictionary() = default;

VariableDictionary::VariableDictionary(VariableDictionary&&) = default;

VariableDictionary& VariableDictionary::operator=(VariableDictionary&&) =
    default;

std::optional<std::string_view> VariableDictionary::Find(
    types::VariableName name) const& {
  auto iter = entries_.find(name.GetName());
  if (iter == entries_.end()) {
    return std::nullopt;
  }

  return *iter->second;
}

bool VariableDictionary::Insert(types::VariableName name, std::string value) {
  return entries_
      .try_emplace(std::move(name).GetName(),
                   std::make_unique<std::string>(std::move(value)))
      .second;
}

ParseStatus::Or<ResolvedSourceString> VariableDictionary::Resolve(
    SourceString input,
    SubstitutionBuffer& buffer) const {
  // Get the first variable reference. If this fails, there were no references
  // and we don't need to allocate anything.
  auto next_var = GetNextVariable(input);
  if (!next_var.tail) {
    return ResolvedSourceString::Create(
        {}, input.Line(), input.Column(), input.Str(),
        ResolvedSourceString::SubstitutionState::kNoSubstitutions);
  }

  // If there was a variable reference, but it consisted of the entire input
  // string, then simply return a reference to the substitution string.
  if (next_var.head.Empty() && next_var.tail->second.Empty()) {
    auto value = Find(next_var.tail->first);
    if (!value) {
      return ParseStatus(ParseStatusCode::kVariableUndefined)
          .WithData("key", next_var.tail->first.GetName());
    }

    return ResolvedSourceString::Create(
        {}, input.Line(), input.Column(), *value,
        ResolvedSourceString::SubstitutionState::kContainsSubstitutions);
  }

  auto& string_buf = buffer.strings_.emplace_back();

  while (true) {
    // Append the substring leading to the variable, and abort if there was no
    // variable reference
    string_buf.append(next_var.head.Str());
    if (!next_var.tail) {
      break;
    }

    // Look up the variable value
    auto value = Find(next_var.tail->first);
    if (!value) {
      return ParseStatus(ParseStatusCode::kVariableUndefined)
          .WithData("key", next_var.tail->first.GetName());
    }
    string_buf.append(*value);

    next_var = GetNextVariable(next_var.tail->second);
  }

  return ResolvedSourceString::Create(
      {}, input.Line(), input.Column(), string_buf,
      ResolvedSourceString::SubstitutionState::kContainsSubstitutions);
}

}  // namespace media::hls
