// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_VARIABLE_DICTIONARY_H_
#define MEDIA_FORMATS_HLS_VARIABLE_DICTIONARY_H_

#include <string>

#include "base/strings/string_piece.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls {

struct SourceString;

namespace types {
class VariableName;
}

class MEDIA_EXPORT VariableDictionary {
 public:
  class SubstitutionBuffer {
   public:
    friend VariableDictionary;
    SubstitutionBuffer() = default;
    SubstitutionBuffer(const SubstitutionBuffer&) = delete;
    SubstitutionBuffer(const SubstitutionBuffer&&) = delete;
    SubstitutionBuffer& operator=(const SubstitutionBuffer&) = delete;
    SubstitutionBuffer& operator=(SubstitutionBuffer&&) = delete;

   private:
    std::string buf_;
  };

  VariableDictionary();
  ~VariableDictionary();
  VariableDictionary(VariableDictionary&&);
  VariableDictionary& operator=(VariableDictionary&&);

  // Attempts to find the value of the given variable in this dictionary.
  // The returned `base::StringPiece` must not outlive this
  // `VariableDictionary`.
  absl::optional<base::StringPiece> Find(types::VariableName name) const&;
  absl::optional<base::StringPiece> Find(types::VariableName name) const&& =
      delete;

  // Attempts to define the variable `name` with the given value. If the
  // definition already exists, returns `false` without modifying the
  // dictionary.
  bool Insert(types::VariableName name, std::string value);

  // Attempts to resolve all variable references within the given input string
  // using this dictionary, returning a `base::StringPiece` with the fully
  // resolved string, or an error if one occurred. `buffer` will be used to
  // build the resulting string if any substitutions occur, and the caller must
  // ensure that it outlives the `base::StringPiece` returned by this function.
  // As an optimization, the buffer will not be used if no substitutions are
  // necessary.
  //
  // This implementation is based on a somewhat pedantic interpretation of the
  // spec:
  //
  //    A Variable Reference is a string of the form "{$" (0x7B,0x24) followed
  //    by a Variable Name followed by "}" (0x7D).
  //
  // If a given sequence doesn't exactly match that format then it's ignored,
  // rather than treated as an error. However, if it does match that format and
  // the variable name is undefined, it's treated as an error.
  ParseStatus::Or<base::StringPiece> Resolve(SourceString input,
                                             SubstitutionBuffer& buffer) const;

 private:
  base::flat_map<std::string, std::string> entries_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_VARIABLE_DICTIONARY_H_
