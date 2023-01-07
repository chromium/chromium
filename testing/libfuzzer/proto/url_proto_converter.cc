// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>
#include <string>

#include "testing/libfuzzer/proto/url.pb.h"

namespace url_proto {

namespace {

std::string SlashToString(int slash) {
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

}  // namespace

std::string Convert(const url_proto::Url& url) {
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
    url_string += SlashToString(slash);

  // Get host. This is simple since hosts are simply strings according to our
  // definition.
  if (url.has_host()) {
    // Get userinfo if libFuzzer set it. Ensure that user is separated
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
  std::string path_separator = SlashToString(url.path_separator());
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
  // ensure that it is preceded by "?". Also separate query components with
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

}  // namespace url_proto
