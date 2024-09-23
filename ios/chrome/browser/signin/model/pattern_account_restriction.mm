// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/pattern_account_restriction.h"

#import <string_view>

#import "base/strings/string_util.h"
#import "base/values.h"

Pattern::Pattern(std::vector<std::string> chunks) : chunks_(chunks) {}
Pattern::~Pattern() = default;

Pattern::Pattern(const Pattern&) = default;

Pattern::Pattern(Pattern&& from) = default;
Pattern& Pattern::operator=(Pattern&& from) = default;

bool Pattern::Match(std::string_view string) const {
  // No wildcards, the whole string should match the pattern.
  if (chunks_.size() == 1) {
    return string.compare(chunks_.front()) == 0;
  }

  // The first chunk should match the string head.
  const std::string& first_chunk = chunks_.front();
  if (string.size() < first_chunk.size() ||
      string.substr(0, first_chunk.size()) != first_chunk)
    return false;

  // The last chunk should match the string tail.
  const std::string& last_chunk = chunks_.back();
  if (string.size() < last_chunk.size() ||
      string.substr(string.size() - last_chunk.size()) != last_chunk)
    return false;

  // Greedy match all the rest of the chunks_, excluding the head and the
  // tail.
  size_t string_offset = first_chunk.size();
  for (const std::string& chunk : chunks_) {
    // Skip first & last chunk as they have been checked before
    // already.
    if (&chunk == &first_chunk || &chunk == &last_chunk)
      continue;

    int offset = string.find(chunk, string_offset);
    if (offset == -1)
      return false;

    string_offset = offset + chunk.size();
  }
  return string_offset + last_chunk.size() <= string.size();
}

PatternAccountRestriction::PatternAccountRestriction() = default;
PatternAccountRestriction::PatternAccountRestriction(
    std::vector<Pattern> patterns)
    : patterns_(std::move(patterns)) {}
PatternAccountRestriction::~PatternAccountRestriction() = default;

PatternAccountRestriction::PatternAccountRestriction(
    PatternAccountRestriction&& from) = default;
PatternAccountRestriction& PatternAccountRestriction::operator=(
    PatternAccountRestriction&& from) = default;

bool PatternAccountRestriction::IsAccountRestricted(
    std::string_view email) const {
  if (patterns_.empty())
    return false;
  for (const auto& pattern : patterns_) {
    if (pattern.Match(email))
      return false;
  }
  return true;
}

bool ArePatternsValid(const base::Value* value) {
  // TODO(crbug.com/40205573): Check if we can use regex instead.
  if (!value->is_list())
    return false;

  for (const base::Value& item : value->GetList()) {
    if (!item.is_string())
      return false;
    auto maybe_pattern = PatternFromString(item.GetString());
    if (!maybe_pattern)
      return false;
  }
  return true;
}

std::optional<PatternAccountRestriction> PatternAccountRestrictionFromValue(
    const base::Value::List& list) {
  std::vector<Pattern> patterns;
  patterns.reserve(list.size());
  for (const base::Value& item : list) {
    if (!item.is_string())
      continue;
    auto maybe_pattern = PatternFromString(item.GetString());
    if (!maybe_pattern)
      continue;
    patterns.push_back(*std::move(maybe_pattern));
  }
  return PatternAccountRestriction(std::move(patterns));
}

std::optional<Pattern> PatternFromString(std::string_view chunk) {
  std::vector<std::string> chunks;
  std::string current_chunk;
  bool escape = false;

  for (const char& c : chunk) {
    if (!escape && c == '\\') {
      // Backslash \: escapes the following character, so '\*' will match
      // literal asterisk only. May also escape characters that don't have any
      // special meaning, so 't' and '\t' are the same patterns.
      escape = true;
      continue;
    }
    if (!escape && c == '*') {
      // Asterisk *: matches arbitrary sequence of characters (including empty
      // sequence).
      chunks.push_back(current_chunk);
      current_chunk = "";
      continue;
    }
    current_chunk.push_back(c);
    escape = false;
  }
  if (escape)
    return std::nullopt;
  chunks.push_back(current_chunk);
  return Pattern(std::move(chunks));
}
