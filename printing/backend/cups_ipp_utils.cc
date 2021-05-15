// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_utils.h"

#include <cups/cups.h>

#include <string>

#include "base/values.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/print_backend_consts.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace printing {

std::unique_ptr<CupsConnection> CreateConnection(
    const base::Value* print_backend_settings) {
  std::string print_server_url_str;
  std::string cups_blocking_str;
  int encryption = HTTP_ENCRYPT_NEVER;
  if (print_backend_settings && print_backend_settings->is_dict()) {
    const std::string* url_from_settings =
        print_backend_settings->FindStringKey(kCUPSPrintServerURL);
    if (url_from_settings)
      print_server_url_str = *url_from_settings;

    const std::string* blocking_from_settings =
        print_backend_settings->FindStringKey(kCUPSBlocking);
    if (blocking_from_settings)
      cups_blocking_str = *blocking_from_settings;

    encryption = print_backend_settings->FindIntKey(kCUPSEncryption)
                     .value_or(HTTP_ENCRYPT_NEVER);
  }

  // CupsConnection can take an empty GURL.
  GURL print_server_url = GURL(print_server_url_str);
  bool cups_blocking = cups_blocking_str == kValueTrue;

  return CupsConnection::Create(print_server_url,
                                static_cast<http_encryption_t>(encryption),
                                cups_blocking);
}

}  // namespace printing
