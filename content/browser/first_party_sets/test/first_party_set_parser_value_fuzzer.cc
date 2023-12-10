// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_set_parser.h"

#include <stdlib.h>
#include <iostream>

#include "content/browser/first_party_sets/test/related_website_sets.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace content {

namespace {

constexpr char kPrimary[] = "primary";
constexpr char kAssociated[] = "associatedSites";
constexpr char kService[] = "serviceSites";
constexpr char kCctld[] = "ccTLDs";
constexpr char kReplacements[] = "replacements";
constexpr char kAdditions[] = "additions";

const std::string kSites[10] = {
    "https://site-0.test", "https://site-1.test", "https://site-2.test",
    "https://site-3.test", "https://site-4.test", "https://site-5.test",
    "https://site-6.test", "https://site-7.test", "https://site-8.test",
    "https://site-9.test",
};

const std::string kCctlds[10] = {
    "https://site-0.cctld", "https://site-1.cctld", "https://site-2.cctld",
    "https://site-3.cctld", "https://site-4.cctld", "https://site-5.cctld",
    "https://site-6.cctld", "https://site-7.cctld", "https://site-8.cctld",
    "https://site-9.cctld",
};

base::Value::Dict ConvertSet(const related_website_sets::proto::Set& set) {
  base::Value::Dict json_set;
  json_set.Set(kPrimary, kSites[set.primary()]);
  for (int site : set.associated()) {
    json_set.EnsureList(kAssociated)->Append(kSites[site]);
  }
  for (int site : set.service()) {
    json_set.EnsureList(kService)->Append(kSites[site]);
  }
  for (const related_website_sets::proto::SitePair& site_pair :
       set.cctld_aliases()) {
    json_set.EnsureDict(kCctld)->Set(kCctlds[site_pair.alias()],
                                     kSites[site_pair.canonical()]);
  }

  return json_set;
}

base::Value::Dict ConvertProto(
    const related_website_sets::proto::Policy& policy) {
  base::Value::Dict dict;
  for (const related_website_sets::proto::Set& set : policy.replacements()) {
    dict.EnsureList(kReplacements)->Append(ConvertSet(set));
  }
  for (const related_website_sets::proto::Set& set : policy.additions()) {
    dict.EnsureList(kAdditions)->Append(ConvertSet(set));
  }

  return dict;
}

}  // namespace

DEFINE_PROTO_FUZZER(const related_website_sets::proto::Policy& input) {
  base::Value::Dict native_input = ConvertProto(input);

  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << native_input.DebugString() << std::endl;
  }

  std::ignore =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(native_input);
}

}  // namespace content
