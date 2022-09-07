// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_LOCAL_SET_DECLARATION_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_LOCAL_SET_DECLARATION_H_

#include <string>

#include "base/strings/string_piece_forward.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace content {

class CONTENT_EXPORT LocalSetDeclaration {
 public:
  LocalSetDeclaration();

  explicit LocalSetDeclaration(
      base::StringPiece use_first_party_set_flag_value);

  ~LocalSetDeclaration();

  LocalSetDeclaration(const LocalSetDeclaration&);
  LocalSetDeclaration& operator=(const LocalSetDeclaration&);
  LocalSetDeclaration(LocalSetDeclaration&&);
  LocalSetDeclaration& operator=(LocalSetDeclaration&&);

  bool empty() const { return !parsed_set_.has_value(); }

  size_t size() const { return empty() ? 0 : GetSet().size(); }

  // Gets the primary site. Must not be called if `empty()` returns true.
  net::SchemefulSite GetPrimary() const;

  // Gets the set entries. Must not be called if `empty()` returns true.
  FirstPartySetParser::SingleSet GetSet() const;

 private:
  explicit LocalSetDeclaration(
      absl::optional<std::pair<net::SchemefulSite,
                               FirstPartySetParser::SingleSet>> parsed_set);

  // Stores the result of parsing the inputs. Specifically, this may be empty if
  // no set was locally defined; otherwise, it holds the primary site, and the
  // collection of FirstPartySetEntries.
  absl::optional<std::pair<net::SchemefulSite, FirstPartySetParser::SingleSet>>
      parsed_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_LOCAL_SET_DECLARATION_H_
