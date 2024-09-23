// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/parsed_content_header_field_parameters.h"

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "third_party/blink/renderer/platform/network/header_field_tokenizer.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

// parameters := *(";" parameter)
//
// From http://tools.ietf.org/html/rfc2045#section-5.1:
//
// parameter := attribute "=" value
//
// attribute := token
//              ; Matching of attributes
//              ; is ALWAYS case-insensitive.
//
// value := token / quoted-string
//
// token := 1*<any (US-ASCII) CHAR except SPACE, CTLs,
//             or tspecials>
//
// tspecials :=  "(" / ")" / "<" / ">" / "@" /
//               "," / ";" / ":" / "\" / <">
//               "/" / "[" / "]" / "?" / "="
//               ; Must be in quoted-string,
//               ; to use within parameter values
std::optional<ParsedContentHeaderFieldParameters>
ParsedContentHeaderFieldParameters::Parse(HeaderFieldTokenizer tokenizer,
                                          Mode mode) {
  NameValuePairs parameters;
  while (!tokenizer.IsConsumed()) {
    if (!tokenizer.Consume(';')) {
      DVLOG(1) << "Failed to find ';'";
      return std::nullopt;
    }

    StringView key;
    String value;
    if (!tokenizer.ConsumeToken(Mode::kNormal, key)) {
      DVLOG(1) << "Invalid content parameter name. (at " << tokenizer.Index()
               << ")";
      return std::nullopt;
    }
    if (!tokenizer.Consume('=')) {
      DVLOG(1) << "Failed to find '='";
      return std::nullopt;
    }
    if (!tokenizer.ConsumeTokenOrQuotedString(mode, value)) {
      DVLOG(1) << "Invalid content parameter value (at " << tokenizer.Index()
               << ", for '" << key.ToString() << "').";
      return std::nullopt;
    }
    parameters.emplace_back(key.ToString(), value);
  }

  return ParsedContentHeaderFieldParameters(std::move(parameters));
}

String ParsedContentHeaderFieldParameters::ParameterValueForName(
    const String& name) const {
  if (!name.ContainsOnlyASCIIOrEmpty())
    return String();

  for (const NameValue& param : base::Reversed(*this)) {
    if (EqualIgnoringASCIICase(param.name, name)) {
      return param.value;
    }
  }
  return String();
}

size_t ParsedContentHeaderFieldParameters::ParameterCount() const {
  return parameters_.size();
}

bool ParsedContentHeaderFieldParameters::HasDuplicatedNames() const {
  HashSet<String> names;
  for (const auto& parameter : parameters_) {
    const String lowered_name = parameter.name.LowerASCII();
    if (names.Contains(lowered_name))
      return true;

    names.insert(lowered_name);
  }
  return false;
}

}  // namespace blink
