// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_utils.h"

#include <cups/cups.h>

#include "printing/backend/cups_connection.h"
#include "url/gurl.h"

namespace printing {

std::unique_ptr<CupsConnection> CreateConnection() {
  // CupsConnection can take an empty GURL.
  return CupsConnection::Create(/*print_server_url=*/GURL(), HTTP_ENCRYPT_NEVER,
                                /*cups_blocking=*/false);
}

}  // namespace printing
