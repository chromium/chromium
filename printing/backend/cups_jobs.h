// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementations of IPP requests for printer queue information.

#ifndef PRINTING_BACKEND_CUPS_JOBS_H_
#define PRINTING_BACKEND_CUPS_JOBS_H_

#include <cups/cups.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/version.h"
#include "printing/printer_query_result.h"

// This file contains a collection of functions used to query IPP printers or
// print servers and the related code to parse these responses.  All Get*
// operations block on the network request and cannot be run on the UI thread.

namespace printing {

struct PrinterStatus;

// Represents a print job sent to the queue.
struct COMPONENT_EXPORT(PRINT_BACKEND) CupsJob {
  // Corresponds to job-state from RFC2911.
  enum JobState {
    UNKNOWN,
    PENDING,  // waiting to be processed
    HELD,  // the job has not begun printing and will not without intervention
    COMPLETED,
    PROCESSING,  // job is being sent to the printer/printed
    STOPPED,     // job was being processed and has now stopped
    CANCELED,    // either the spooler or a user canclled the job
    ABORTED      // an error occurred causing the printer to give up
  };

  // Possible reasons sent by CUPS in job-state-reason
  // The strings are hardcoded in CUPS code and these strings the only once are
  // currently needed.
  enum class JobStateReason {
    kJobCompletedWithErrors = 0,
    kCupsHeldForAuthentication,
  };

  CupsJob();
  CupsJob(const CupsJob& other);
  ~CupsJob();

  // Returns true if `job`.state_reasons contains `reason`.
  bool ContainsStateReason(JobStateReason reason) const;

  // job id
  int id = -1;
  // printer name in CUPS
  std::string printer_id;
  JobState state = UNKNOWN;
  // the last page printed
  int current_pages = -1;
  // detail for the job state
  std::vector<std::string> state_reasons;
  // human readable message explaining the state
  std::string state_message;
  // most recent timestamp where the job entered PROCESSING
  int processing_started = 0;
};

struct COMPONENT_EXPORT(PRINT_BACKEND) PrinterInfo {
  PrinterInfo();
  PrinterInfo(const PrinterInfo& info);

  ~PrinterInfo();

  // printer-make-and-model
  std::string make_and_model;

  // document-format-supported
  // MIME types for supported formats.
  std::vector<std::string> document_formats;

  // ipp-versions-supported
  // A collection of supported IPP protocol versions.
  std::vector<base::Version> ipp_versions;

  // Does ipp-features-supported contain 'ipp-everywhere'.
  bool ipp_everywhere = false;

  // URI of OAuth2 Authorization Server and scope. Empty strings if not set.
  std::string oauth_server;
  std::string oauth_scope;
};

// Specifies classes of jobs.
enum JobCompletionState {
  COMPLETED,  // only completed jobs
  PROCESSING  // only jobs that are being processed
};

// Converts a JobStateReason to the exact string returned by CUPS.
const std::string_view COMPONENT_EXPORT(PRINT_BACKEND)
    ToJobStateReasonString(CupsJob::JobStateReason stateReason);

// Returns the uri for printer with `id` as served by CUPS. Assumes that `id` is
// a valid CUPS printer name and performs no error checking or escaping.
std::string COMPONENT_EXPORT(PRINT_BACKEND)
    PrinterUriFromName(const std::string& id);

// Extracts structured job information from the `response` for `printer_id`.
// Extracted jobs are added to `jobs`.
void ParseJobsResponse(ipp_t* response,
                       const std::string& printer_id,
                       std::vector<CupsJob>* jobs);

// Attempts to extract a PrinterStatus object out of `response`.
void ParsePrinterStatus(ipp_t* response, PrinterStatus* printer_status);

// Queries the printer at `address` on `port` with a Get-Printer-Attributes
// request to populate `printer_info` and `printer_status`. If `encrypted` is
// true, request is made using ipps, otherwise, ipp is used.
PrinterQueryResult COMPONENT_EXPORT(PRINT_BACKEND)
    GetPrinterInfo(const std::string& address,
                   int port,
                   const std::string& resource,
                   bool encrypted,
                   PrinterInfo* printer_info,
                   PrinterStatus* printer_status);

// Attempts to retrieve printer status using connection `http` for `printer_id`.
// Returns true if succcssful and updates the fields in `printer_status` as
// appropriate.  Returns false if the request failed.
bool GetPrinterStatus(http_t* http,
                      const std::string& printer_id,
                      PrinterStatus* printer_status);

// Attempts to retrieve job information using connection `http` for the printer
// named `printer_id`.  Retrieves at most `limit` jobs.  If `completed` then
// completed jobs are retrieved.  Otherwise, jobs that are currently in progress
// are retrieved.  Results are added to `jobs` if the operation was successful.
bool GetCupsJobs(http_t* http,
                 const std::string& printer_id,
                 int limit,
                 JobCompletionState completed,
                 std::vector<CupsJob>* jobs);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_JOBS_H_
