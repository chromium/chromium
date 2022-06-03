// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>

#include <iostream>

// Includes copied from url_parse_fuzzer.cc
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "url/gurl.h"

// Includes *not* copied from url_parse_fuzzer.cc
// Contains DEFINE_BINARY_PROTO_FUZZER, a macro we use to define our target
// function.
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
// Header information about the Protocol Buffer Url class.
#include "testing/libfuzzer/proto/url.pb.h"

// The code using TestCase is copied from url_parse_fuzzer.cc
struct TestCase {
  TestCase() {
    CHECK(base::i18n::InitializeICU());
  }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();

// Silence logging from the protobuf library.
protobuf_mutator::protobuf::LogSilencer log_silencer;

std::string Slash_to_string(int slash) {
  if (slash == url_proto::Url::NONE)
    return "";
  if (slash == url_proto::Url::FORWARD)
    return "/";
  if (slash == url_proto::Url::BACKWARD) {
    return "\\";
  }
  assert(false && "Received unexpected value for slash");
  // Silence compiler warning about not returning in non-void function.
  return "";
}

// Converts a URL in Protocol Buffer format to a url in string format.
// Since protobuf is a relatively simple format, fuzzing targets that do not
// accept protobufs (such as this one) will require code to convert from
// protobuf to the accepted format (string in this case).
std::string protobuf_to_string(const url_proto::Url& url) {
  // Build url_string piece by piece from url and then return it.
  std::string url_string = std::string("");

  if (url.has_scheme()) {  // Get the scheme if Url has it.
    // Append the scheme to the url. This may be empty. Then append a colon
    // which is mandatory if there is a scheme.
    url_string += url.scheme() + ":";
  }

  // Just append the slashes without doing validation, since it would be too
  // complex. libFuzzer will hopefully figure out good values.
  for (const int slash : url.slashes())
    url_string += Slash_to_string(slash);

  // Get host. This is simple since hosts are simply strings according to our
  // definition.
  if (url.has_host()) {
    // Get userinfo if libFuzzer set it. Ensure that user is seperated
    // from the password by ":" (if a password is included) and that userinfo is
    // separated from the host by "@".
    if (url.has_userinfo()) {
      url_string += url.userinfo().user();
      if (url.userinfo().has_password()) {
        url_string += ":";
        url_string += url.userinfo().password();
      }
      url_string += "@";
    }
    url_string += url.host();

    // As explained in url.proto, if libFuzzer included a port in url ensure
    // that it is preceded by the host and then ":".
    if (url.has_port())
      // Convert url.port() from an unsigned 32 bit int before appending it.
      url_string += ":" + std::to_string(url.port());
  }

  // Append the path segments to the url, with each segment separated by
  // the path_separator.
  bool first_segment = true;
  std::string path_separator = Slash_to_string(url.path_separator());
  for (const std::string& path_segment : url.path()) {
    // There does not need to be a path, but if there is a path and a host,
    // ensure the path begins with "/".
    if (url.has_host() && first_segment) {
      url_string += "/" + path_segment;
      first_segment = false;
    } else
      url_string += path_separator + path_segment;
  }

  // Queries must be started by "?". If libFuzzer included a query in url,
  // ensure that it is preceded by "?". Also Seperate query components with
  // ampersands as is the convention.
  bool first_component = true;
  for (const std::string& query_component : url.query()) {
    if (first_component) {
      url_string += "?" + query_component;
      first_component = false;
    } else
      url_string += "&" + query_component;
  }

  // Fragments must be started by "#". If libFuzzer included a fragment
  // in url, ensure that it is preceded by "#".
  if (url.has_fragment())
    url_string += "#" + url.fragment();

  return url_string;
}

// The target function. This is the equivalent of LLVMFuzzerTestOneInput in
// typical libFuzzer based fuzzers. It is passed our Url protobuf object that
// was mutated by libFuzzer, converts it to a string and then feeds it to url()
// for fuzzing.
DEFINE_BINARY_PROTO_FUZZER(const url_proto::Url& url_protobuf) {
  std::string url_string = protobuf_to_string(url_protobuf);

  // Allow native input to be retrieved easily.
  // Note that there will be a trailing newline that is not part of url_string.
  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << url_string << std::endl;

  GURL url(url_string);
}
