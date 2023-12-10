// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/schemeful_site.h"

#include <stdlib.h>

#include <iostream>
#include <optional>
#include <string>

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/proto/url.pb.h"
#include "testing/libfuzzer/proto/url_proto_converter.h"
#include "url/gurl.h"
#include "url/origin.h"

DEFINE_PROTO_FUZZER(const url_proto::Url& url_message) {
  std::string native_input = url_proto::Convert(url_message);

  if (getenv("LPM_DUMP_NATIVE_INPUT"))
    std::cout << native_input << std::endl;

  url::Origin origin = url::Origin::Create((GURL(native_input)));

  // We don't run the fuzzer on inputs whose hosts will contain "..". The ".."
  // causes SchemefulSite to consider the registrable domain to start with the
  // second ".".
  if (origin.host().find("..") != std::string::npos)
    return;

  net::SchemefulSite site(origin);

  std::optional<net::SchemefulSite> site_with_registrable_domain =
      net::SchemefulSite::CreateIfHasRegisterableDomain(origin);

  if (site_with_registrable_domain) {
    CHECK_EQ(site_with_registrable_domain->GetInternalOriginForTesting(),
             site.GetInternalOriginForTesting());
    CHECK(site.has_registrable_domain_or_host());
    const std::string& scheme = site.GetInternalOriginForTesting().scheme();
    if (scheme == "http" || scheme == "https") {
      CHECK_NE(site.registrable_domain_or_host_for_testing().front(), '.');
    }
  }
}
