// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/schemeful_site.h"

#include <stdlib.h>
#include <iostream>
#include <string>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/proto/url.pb.h"
#include "testing/libfuzzer/proto/url_proto_converter.h"
#include "url/gurl.h"

namespace {
void RunAssertions(const net::SchemefulSite& site) {
  if (site.has_registrable_domain_or_host()) {
    CHECK_NE(site.registrable_domain_or_host_for_testing().front(), '.');
  }
}
}  // namespace

DEFINE_PROTO_FUZZER(const url_proto::Url& url_message) {
  std::string native_input = url_proto::Convert(url_message);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  net::SchemefulSite site((GURL(native_input)));

  RunAssertions(site);
}
