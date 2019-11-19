// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_

#include <stddef.h>
#include <string>

#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

// Holds the ParseResult together with the id of the rule at which the error
// occurred, if any.
class ParseInfo {
 public:
  explicit ParseInfo(ParseResult result);
  ParseInfo(ParseResult result, int rule_id);
  ParseInfo(const ParseInfo&);
  ParseInfo& operator=(const ParseInfo&);

  ParseResult result() const { return result_; }

  // Returns the error string corresponding to this ParseInfo. Should not be
  // called on a successful parse.
  std::string GetErrorDescription() const;

 private:
  ParseResult result_;
  // When set, denotes the id of the rule with which the |result_| is
  // associated.
  base::Optional<int> rule_id_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
