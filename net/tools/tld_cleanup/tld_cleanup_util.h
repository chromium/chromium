// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TLD_CLEANUP_TLD_CLEANUP_UTIL_H_
#define NET_TOOLS_TLD_CLEANUP_TLD_CLEANUP_UTIL_H_

#include <map>
#include <string>
#include <tuple>

namespace base {
class FilePath;
}  // namespace base

namespace net::tld_cleanup {

struct Rule {
  bool exception;
  bool wildcard;
  bool is_private;

  bool operator==(const Rule& other) const {
    return std::tie(exception, wildcard, is_private) ==
           std::tie(other.exception, other.wildcard, other.is_private);
  }
};

typedef std::map<std::string, Rule> RuleMap;

// These result codes should be in increasing order of severity.
enum class NormalizeResult {
  kSuccess,
  kWarning,
  kError,
};

// Converts the list of domain rules contained in the `rules` map to string.
// Rule lines all have trailing LF in the output.
std::string RulesToGperf(const RuleMap& rules);

// Loads the file described by |in_filename|, converts it to the desired format
// (see the file comments in tld_cleanup.cc), and saves it into |out_filename|.
// Returns the most severe of the result codes encountered when normalizing the
// rules.
NormalizeResult NormalizeFile(const base::FilePath& in_filename,
                              const base::FilePath& out_filename);

// Parses |data|, and converts it to the internal data format RuleMap. Returns
// the most severe of the result codes encountered when normalizing the rules.
NormalizeResult NormalizeDataToRuleMap(const std::string& data, RuleMap& rules);

}  // namespace net::tld_cleanup

#endif  // NET_TOOLS_TLD_CLEANUP_TLD_CLEANUP_UTIL_H_
