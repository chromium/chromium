// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_VARIABLE_DICTIONARY_H_
#define MEDIA_FORMATS_HLS_VARIABLE_DICTIONARY_H_

#include <list>
#include <optional>
#include <string>
#include <string_view>

#include "base/types/pass_key.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"

namespace media::hls {

namespace types {
class VariableName;
}

class MEDIA_EXPORT VariableDictionary {
 public:
  class MEDIA_EXPORT SubstitutionBuffer {
   public:
    friend VariableDictionary;
    SubstitutionBuffer();
    ~SubstitutionBuffer();
    SubstitutionBuffer(const SubstitutionBuffer&) = delete;
    SubstitutionBuffer(const SubstitutionBuffer&&) = delete;
    SubstitutionBuffer& operator=(const SubstitutionBuffer&) = delete;
    SubstitutionBuffer& operator=(SubstitutionBuffer&&) = delete;

   private:
    std::list<std::string> strings_;
  };

  VariableDictionary();
  ~VariableDictionary();
  VariableDictionary(VariableDictionary&&);
  VariableDictionary& operator=(VariableDictionary&&);

  // Attempts to find the value of the given variable in this dictionary.
  // The returned `std::string_view` must not outlive this
  // `VariableDictionary`.
  std::optional<std::string_view> Find(types::VariableName name) const&;
  std::optional<std::string_view> Find(types::VariableName name) const&& =
      delete;

  // Attempts to define the variable `name` with the given value. If the
  // definition already exists, returns `false` without modifying the
  // dictionary.
  bool Insert(types::VariableName name, std::string value);

  // Attempts to resolve all variable references within the given input string
  // using this dictionary, returning a `ResolvedSourceString` with the fully
  // resolved string, or an error if one occurred. `buffer` will be used to
  // build the resulting string if any substitutions occur, and the caller must
  // ensure that it outlives the `ResolvedSourceString` returned by this
  // function. As an optimization, the buffer will not be used if no
  // substitutions are necessary, or if the substitution consisted of the entire
  // input string.
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
  ParseStatus::Or<ResolvedSourceString> Resolve(
      SourceString input,
      SubstitutionBuffer& buffer) const;

 private:
  base::flat_map<std::string, std::unique_ptr<std::string>> entries_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_VARIABLE_DICTIONARY_H_
