/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_TEST_HARNESS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_TEST_HARNESS_H_

#include <istream>
#include <map>
#include <stdio.h>
#include <string>

// FIXME: We don't have any business owning this code. We should try to
// upstream this to unicode.org if possible (for other implementations to use).
// Unicode.org provides a reference implmentation, including parser:
// http://www.unicode.org/Public/PROGRAMS/BidiReferenceC/6.3.0/source/brtest.c
// But it, like the other implementations I've found, is rather tied to
// the algorithms it is testing. This file seeks to only implement the parser
// bits.

// Other C/C++ implementations of this parser:
// https://github.com/googlei18n/fribidi-vs-unicode/blob/master/test.c
// http://source.icu-project.org/repos/icu/icu/trunk/source/test/intltest/bidiconf.cpp
// Both of those are too tied to their respective projects to be use to Blink.

// There are non-C implmentations to parse BidiTest.txt as well, including:
// https://github.com/twitter/twitter-cldr-rb/blob/master/spec/bidi/bidi_spec.rb

// NOTE: None of this file is currently written to be thread-safe.

namespace bidi_test {

enum ParagraphDirection {
  kDirectionNone = 0,
  kDirectionAutoLTR = 1,
  kDirectionLTR = 2,
  kDirectionRTL = 4,
};
const int kMaxParagraphDirection =
    kDirectionAutoLTR | kDirectionLTR | kDirectionRTL;

// For error printing:
std::string NameFromParagraphDirection(ParagraphDirection paragraph_direction) {
  switch (paragraph_direction) {
    case bidi_test::kDirectionAutoLTR:
      return "Auto-LTR";
    case bidi_test::kDirectionLTR:
      return "LTR";
    case bidi_test::kDirectionRTL:
      return "RTL";
    default:
      // This should never be reached.
      return "";
  }
}

template <class Runner>
class Harness {
 public:
  Harness(Runner& runner) : runner_(runner) {}
  void Parse(std::istream& bidi_test_file);

 private:
  Runner& runner_;
};

// We could use boost::trim, but no other part of Blink uses boost yet.
inline void Ltrim(std::string& s) {
  static const std::string kSeparators(" \t");
  s.erase(0, s.find_first_not_of(kSeparators));
}

inline void Rtrim(std::string& s) {
  static const std::string kSeparators(" \t");
  size_t last_non_space = s.find_last_not_of(kSeparators);
  if (last_non_space == std::string::npos) {
    s.erase();
    return;
  }
  size_t first_space_at_end_of_string = last_non_space + 1;
  if (first_space_at_end_of_string >= s.size())
    return;  // lastNonSpace was the last char.
  s.erase(first_space_at_end_of_string,
          std::string::npos);  // erase to the end of the string.
}

inline void Trim(std::string& s) {
  Rtrim(s);
  Ltrim(s);
}

static Vector<std::string> ParseStringList(const std::string& str) {
  Vector<std::string> strings;
  static const std::string kSeparators(" \t");
  size_t last_pos = str.find_first_not_of(kSeparators);   // skip leading spaces
  size_t pos = str.find_first_of(kSeparators, last_pos);  // find next space

  while (std::string::npos != pos || std::string::npos != last_pos) {
    strings.push_back(str.substr(last_pos, pos - last_pos));
    last_pos = str.find_first_not_of(kSeparators, pos);
    pos = str.find_first_of(kSeparators, last_pos);
  }
  return strings;
}

static int ParseInt(const std::string& str) {
  return atoi(str.c_str());
}

static Vector<int> ParseIntList(const std::string& str) {
  Vector<int> ints;
  Vector<std::string> strings = ParseStringList(str);
  for (size_t x = 0; x < strings.size(); x++) {
    int i = ParseInt(strings[x]);
    ints.push_back(i);
  }
  return ints;
}

static Vector<int> ParseLevels(const std::string& line) {
  Vector<int> levels;
  Vector<std::string> strings = ParseStringList(line);
  for (size_t x = 0; x < strings.size(); x++) {
    const std::string& level_string = strings[x];
    int i;
    if (level_string == "x")
      i = -1;
    else
      i = ParseInt(level_string);
    levels.push_back(i);
  }
  return levels;
}

// This is not thread-safe as written.
static std::basic_string<UChar> ParseTestString(const std::string& line) {
  std::basic_string<UChar> test_string;
  static std::map<std::string, UChar> char_class_examples;
  if (char_class_examples.empty()) {
    char_class_examples = {{"L", 0x6c},      // 'l' for L
                           {"R", 0x05D0},    // HEBREW ALEF
                           {"EN", 0x33},     // '3' for EN
                           {"ES", 0x2d},     // '-' for ES
                           {"ET", 0x25},     // '%' for ET
                           {"AN", 0x0660},   // arabic 0
                           {"CS", 0x2c},     // ',' for CS
                           {"B", 0x0A},      // <control-000A>
                           {"S", 0x09},      // <control-0009>
                           {"WS", 0x20},     // ' ' for WS
                           {"ON", 0x3d},     // '=' for ON
                           {"NSM", 0x05BF},  // HEBREW POINT RAFE
                           {"AL", 0x0608},   // ARABIC RAY
                           {"BN", 0x00AD},   // SOFT HYPHEN
                           {"LRE", 0x202A}, {"RLE", 0x202B}, {"PDF", 0x202C},
                           {"LRO", 0x202D}, {"RLO", 0x202E}, {"LRI", 0x2066},
                           {"RLI", 0x2067}, {"FSI", 0x2068}, {"PDI", 0x2069}};
  }

  Vector<std::string> char_classes = ParseStringList(line);
  for (size_t i = 0; i < char_classes.size(); i++) {
    // FIXME: If the lookup failed we could return false for a parse error.
    test_string.push_back(char_class_examples.find(char_classes[i])->second);
  }
  return test_string;
}

static bool ParseParagraphDirectionMask(const std::string& line,
                                        int& mode_mask) {
  mode_mask = ParseInt(line);
  return mode_mask >= 1 && mode_mask <= kMaxParagraphDirection;
}

static void ParseError(const std::string& line, size_t line_number) {
  // Use printf to avoid the expense of std::cout.
  printf("Parse error, line %zu : %s\n", line_number, line.c_str());
}

template <class Runner>
void Harness<Runner>::Parse(std::istream& bidi_test_file) {
  static const std::string kLevelsPrefix("@Levels");
  static const std::string kReorderPrefix("@Reorder");

  // FIXME: UChar is an ICU type and cheating a bit to use here.
  // uint16_t might be more portable.
  std::basic_string<UChar> test_string;
  Vector<int> levels;
  Vector<int> reorder;
  int paragraph_direction_mask;

  std::string line;
  size_t line_number = 0;
  while (std::getline(bidi_test_file, line)) {
    line_number++;
    const std::string original_line = line;
    size_t comment_start = line.find_first_of('#');
    if (comment_start != std::string::npos)
      line = line.substr(0, comment_start);
    Trim(line);
    if (line.empty())
      continue;
    if (line[0] == '@') {
      if (!line.find(kLevelsPrefix)) {
        levels = ParseLevels(line.substr(kLevelsPrefix.length() + 1));
        continue;
      }
      if (!line.find(kReorderPrefix)) {
        reorder = ParseIntList(line.substr(kReorderPrefix.length() + 1));
        continue;
      }
    } else {
      // Assume it's a data line.
      size_t seperator_index = line.find_first_of(';');
      if (seperator_index == std::string::npos) {
        ParseError(original_line, line_number);
        continue;
      }
      test_string = ParseTestString(line.substr(0, seperator_index));
      if (!ParseParagraphDirectionMask(line.substr(seperator_index + 1),
                                       paragraph_direction_mask)) {
        ParseError(original_line, line_number);
        continue;
      }

      if (paragraph_direction_mask & kDirectionAutoLTR) {
        runner_.RunTest(test_string, reorder, levels, kDirectionAutoLTR,
                         original_line, line_number);
      }
      if (paragraph_direction_mask & kDirectionLTR) {
        runner_.RunTest(test_string, reorder, levels, kDirectionLTR,
                         original_line, line_number);
      }
      if (paragraph_direction_mask & kDirectionRTL) {
        runner_.RunTest(test_string, reorder, levels, kDirectionRTL,
                         original_line, line_number);
      }
    }
  }
}

template <class Runner>
class CharacterHarness {
 public:
  CharacterHarness(Runner& runner) : runner_(runner) {}
  void Parse(std::istream& bidi_test_file);

 private:
  Runner& runner_;
};

static std::basic_string<UChar> ParseUCharHexadecimalList(
    const std::string& str) {
  std::basic_string<UChar> string;
  Vector<std::string> strings = ParseStringList(str);
  for (size_t x = 0; x < strings.size(); x++) {
    int i = strtol(strings[x].c_str(), nullptr, 16);
    string.push_back((UChar)i);
  }
  return string;
}

static ParagraphDirection ParseParagraphDirection(const std::string& str) {
  int i = ParseInt(str);
  switch (i) {
    case 0:
      return kDirectionLTR;
    case 1:
      return kDirectionRTL;
    case 2:
      return kDirectionAutoLTR;
    default:
      return kDirectionNone;
  }
}

static int ParseSuppresedChars(const std::string& str) {
  Vector<std::string> strings = ParseStringList(str);
  int suppresed_chars = 0;
  for (size_t x = 0; x < strings.size(); x++) {
    if (strings[x] == "x")
      suppresed_chars++;
  }
  return suppresed_chars;
}

template <class Runner>
void CharacterHarness<Runner>::Parse(std::istream& bidi_test_file) {
  std::string line;
  size_t line_number = 0;
  while (std::getline(bidi_test_file, line)) {
    line_number++;

    const std::string original_line = line;
    size_t comment_start = line.find_first_of('#');
    if (comment_start != std::string::npos)
      line = line.substr(0, comment_start);
    Trim(line);
    if (line.empty())
      continue;

    // Field 0: list of uchars as 4 char strings
    size_t separator_index = line.find_first_of(';');
    if (separator_index == std::string::npos) {
      ParseError(original_line, line_number);
      continue;
    }

    std::basic_string<UChar> test_string =
        ParseUCharHexadecimalList(line.substr(0, separator_index));
    if (test_string.empty()) {
      ParseError(original_line, line_number);
      continue;
    }
    line = line.substr(separator_index + 1);

    // Field 1: paragraph direction (0 LTR, 1 RTL, 2 AutoLTR)
    separator_index = line.find_first_of(';');
    if (separator_index == std::string::npos) {
      ParseError(original_line, line_number);
      continue;
    }

    ParagraphDirection paragraph_direction =
        ParseParagraphDirection(line.substr(0, separator_index));
    if (paragraph_direction == kDirectionNone) {
      ParseError(original_line, line_number);
      continue;
    }
    line = line.substr(separator_index + 1);

    // Field 2: resolved paragraph embedding level
    separator_index = line.find_first_of(';');
    if (separator_index == std::string::npos) {
      ParseError(original_line, line_number);
      continue;
    }

    int paragraph_embedding_level = ParseInt(line.substr(0, separator_index));
    if (paragraph_embedding_level < 0) {
      ParseError(original_line, line_number);
      continue;
    }
    line = line.substr(separator_index + 1);

    // Field 3: List of resolved levels
    separator_index = line.find_first_of(';');
    if (separator_index == std::string::npos) {
      ParseError(original_line, line_number);
      continue;
    }

    int supressed_chars = ParseSuppresedChars(line.substr(0, separator_index));
    Vector<int> levels = ParseLevels(line.substr(0, separator_index));
    if (test_string.size() != levels.size()) {
      ParseError(original_line, line_number);
      continue;
    }
    line = line.substr(separator_index + 1);

    // Field 4: visual ordering of characters
    separator_index = line.find_first_of(';');
    if (separator_index != std::string::npos) {
      ParseError(original_line, line_number);
      continue;
    }

    Vector<int> visual_ordering = ParseIntList(line);
    if (test_string.size() - supressed_chars != visual_ordering.size()) {
      ParseError(original_line, line_number);
      continue;
    }

    runner_.RunTest(test_string, visual_ordering, levels, paragraph_direction,
                     original_line, line_number);
  }
}

}  // namespace bidi_test

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_BIDI_TEST_HARNESS_H_
