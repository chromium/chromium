// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_CONNECTION_H_
#define PRINTING_BACKEND_CUPS_CONNECTION_H_

#include <cups/cups.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "printing/backend/cups_deleters.h"
#include "printing/backend/cups_jobs.h"
#include "printing/backend/cups_printer.h"
#include "printing/printing_export.h"
#include "url/gurl.h"

namespace printing {

// Represents the status of a printer queue.
struct PRINTING_EXPORT QueueStatus {
  QueueStatus();
  QueueStatus(const QueueStatus& other);
  ~QueueStatus();

  PrinterStatus printer_status;
  std::vector<CupsJob> jobs;
};

// Represents a connection to a CUPS server.
class PRINTING_EXPORT CupsConnection {
 public:
  CupsConnection(const GURL& print_server_url,
                 http_encryption_t encryption,
                 bool blocking);

  CupsConnection(CupsConnection&& connection);

  ~CupsConnection();

  // Returns a vector of all the printers configure on the CUPS server.
  std::vector<CupsPrinter> GetDests();

  // Returns a printer for |printer_name| from the connected server.
  std::unique_ptr<CupsPrinter> GetPrinter(const std::string& printer_name);

  // Queries CUPS for printer queue status for |printer_ids|.  Populates |jobs|
  // with said information with one QueueStatus per printer_id.  Returns true if
  // all the queries were successful.  In the event of failure, |jobs| will be
  // unchanged.
  bool GetJobs(const std::vector<std::string>& printer_ids,
               std::vector<QueueStatus>* jobs);

  // Queries CUPS for printer status for |printer_id|.
  // Returns true if the query was successful.
  bool GetPrinterStatus(const std::string& printer_id,
                        PrinterStatus* printer_status);

  std::string server_name() const;

  int last_error() const;

 private:
  // lazily initialize http connection
  bool Connect();

  GURL print_server_url_;
  http_encryption_t cups_encryption_;
  bool blocking_;

  ScopedHttpPtr cups_http_;

  DISALLOW_COPY_AND_ASSIGN(CupsConnection);
};

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_CONNECTION_H_
