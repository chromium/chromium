// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stdlib.h>

#include <iostream>

#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/test/related_website_sets.pb.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace content {

namespace {

constexpr char kPrimary[] = "primary";
constexpr char kAssociated[] = "associatedSites";
constexpr char kService[] = "serviceSites";
constexpr char kCctld[] = "ccTLDs";
constexpr char kReplacements[] = "replacements";
constexpr char kAdditions[] = "additions";

constexpr char const* kSites[10] = {
    "https://site-0.test", "https://site-1.test", "https://site-2.test",
    "https://site-3.test", "https://site-4.test", "https://site-5.test",
    "https://site-6.test", "https://site-7.test", "https://site-8.test",
    "https://site-9.test",
};

constexpr char const* kCctlds[10] = {
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

std::string ConvertProto(
    const related_website_sets::proto::PublicSets& public_sets) {
  std::string out;

  for (const related_website_sets::proto::Set& set : public_sets.sets()) {
    base::StrAppend(&out, {base::WriteJson(ConvertSet(set)).value()});
  }

  return out;
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

std::string ConvertProto(
    const related_website_sets::proto::CommandLineSwitch& command_line_switch) {
  std::string out;

  if (command_line_switch.has_set()) {
    base::StrAppend(
        &out, {base::WriteJson(ConvertSet(command_line_switch.set())).value()});
  }

  return out;
}

struct NativeInputs {
  std::string public_sets;
  base::Value::Dict policy;
  std::string command_line_switch;
};

NativeInputs ConvertProto(const related_website_sets::proto::AllInputs& input) {
  return NativeInputs{
      ConvertProto(input.public_sets()),
      ConvertProto(input.policy()),
      ConvertProto(input.command_line_switch()),
  };
}

}  // namespace

DEFINE_PROTO_FUZZER(const related_website_sets::proto::AllInputs& input) {
  NativeInputs native_inputs = ConvertProto(input);

  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    std::cout << native_inputs.public_sets << std::endl;
    std::cout << native_inputs.policy.DebugString() << std::endl;
    std::cout << native_inputs.command_line_switch << std::endl;
  }

  std::istringstream stream(native_inputs.public_sets);
  net::GlobalFirstPartySets global_sets =
      FirstPartySetParser::ParseSetsFromStream(stream, base::Version("1.0"),
                                               false, false);

  auto [parsed_policy, warnings] =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(native_inputs.policy);

  net::LocalSetDeclaration local_set_declaration =
      FirstPartySetParser::ParseFromCommandLine(
          native_inputs.command_line_switch);

  global_sets.ApplyManuallySpecifiedSet(local_set_declaration);
  if (parsed_policy.has_value()) {
    global_sets.ComputeConfig(parsed_policy.value().mutation());
  }
}

}  // namespace content
