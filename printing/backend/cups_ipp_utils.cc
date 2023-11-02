// Copyright 2020 The Chromium Authors
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
    const base::Value::Dict* print_backend_settings) {
  std::string print_server_url_str;
  bool cups_blocking = false;
  int encryption = HTTP_ENCRYPT_NEVER;
  if (print_backend_settings) {
    const std::string* url_from_settings =
        print_backend_settings->FindString(kCUPSPrintServerURL);
    if (url_from_settings)
      print_server_url_str = *url_from_settings;

    const std::string* blocking_from_settings =
        print_backend_settings->FindString(kCUPSBlocking);
    if (blocking_from_settings)
      cups_blocking = *blocking_from_settings == kValueTrue;

    encryption = print_backend_settings->FindInt(kCUPSEncryption)
                     .value_or(HTTP_ENCRYPT_NEVER);
  }

  // CupsConnection can take an empty GURL.
  GURL print_server_url = GURL(print_server_url_str);

  return CupsConnection::Create(print_server_url,
                                static_cast<http_encryption_t>(encryption),
                                cups_blocking);
}

}  // namespace printing
