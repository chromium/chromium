// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_CERT_VERIFY_TOOL_VERIFY_USING_PATH_BUILDER_H_
#define NET_TOOLS_CERT_VERIFY_TOOL_VERIFY_USING_PATH_BUILDER_H_

#include <vector>

#include "base/memory/scoped_refptr.h"

namespace base {
class FilePath;
class Time;
}

namespace net {
class CertNetFetcher;
class SystemTrustStore;
}

struct CertInput;
struct CertInputWithTrustSetting;

// Verifies |target_der_cert| using bssl::CertPathBuilder. Returns true if the
// certificate verified successfully, false if it failed to verify or there was
// some other error.
// Informational messages will be printed to stdout/stderr as appropriate.
bool VerifyUsingPathBuilder(
    const CertInput& target_der_cert,
    const std::vector<CertInput>& intermediate_der_certs,
    const std::vector<CertInputWithTrustSetting>& der_certs_with_trust_settings,
    const base::Time at_time,
    const base::FilePath& dump_prefix_path,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    net::SystemTrustStore* system_trust_store);

#endif  // NET_TOOLS_CERT_VERIFY_TOOL_VERIFY_USING_PATH_BUILDER_H_
