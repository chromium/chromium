// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_CONNECTION_H_
#define PRINTING_BACKEND_CUPS_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "printing/backend/cups_deleters.h"
#include "printing/backend/cups_jobs.h"
#include "printing/backend/cups_printer.h"
#include "printing/printer_status.h"

namespace printing {

// Represents the status of a printer queue.
struct COMPONENT_EXPORT(PRINT_BACKEND) QueueStatus {
  QueueStatus();
  QueueStatus(const QueueStatus& other);
  ~QueueStatus();

  PrinterStatus printer_status;
  std::vector<CupsJob> jobs;
};

// Represents a connection to a CUPS server.
class COMPONENT_EXPORT(PRINT_BACKEND) CupsConnection {
 public:
  virtual ~CupsConnection() = default;

  static std::unique_ptr<CupsConnection> Create();

  // Obtain a vector of all the printers configure on the CUPS server.  Returns
  // true if the list of printers was obtained, and false if an error was
  // encountered during the query.
  virtual bool GetDests(
      std::vector<std::unique_ptr<CupsPrinter>>& printers) = 0;

  // Returns a printer for `printer_name` from the connected server.
  virtual std::unique_ptr<CupsPrinter> GetPrinter(
      const std::string& printer_name) = 0;

  // Queries CUPS for printer queue status for `printer_ids`.  Populates `jobs`
  // with said information with one QueueStatus per printer_id.  Returns true if
  // all the queries were successful.  In the event of failure, `jobs` will be
  // unchanged.
  virtual bool GetJobs(const std::vector<std::string>& printer_ids,
                       std::vector<QueueStatus>* jobs) = 0;

  // Queries CUPS for printer status for `printer_id`.
  // Returns true if the query was successful.
  virtual bool GetPrinterStatus(const std::string& printer_id,
                                PrinterStatus* printer_status) = 0;

  virtual int last_error() const = 0;
  virtual std::string last_error_message() const = 0;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_CONNECTION_H_
