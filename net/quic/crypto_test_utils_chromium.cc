// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto_test_utils_chromium.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/ssl/ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/test_ticket_crypter.h"

using std::string;

namespace net::test {

std::unique_ptr<quic::ProofSource> ProofSourceForTestingChromium() {
  auto source = std::make_unique<net::ProofSourceChromium>();
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  CHECK(source->Initialize(certs_dir.AppendASCII("quic-chain.pem"),
                           certs_dir.AppendASCII("quic-leaf-cert.key"),
                           certs_dir.AppendASCII("quic-leaf-cert.key.sct")));
  source->SetTicketCrypter(std::make_unique<quic::test::TestTicketCrypter>());
  return std::move(source);
}

}  // namespace net::test
