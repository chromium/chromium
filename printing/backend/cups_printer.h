// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_PRINTER_H_
#define PRINTING_BACKEND_CUPS_PRINTER_H_

#include <cups/cups.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "printing/backend/cups_deleters.h"
#include "url/gurl.h"

namespace printing {

struct PrinterBasicInfo;

// Provides information regarding cups options.
class COMPONENT_EXPORT(PRINT_BACKEND) CupsOptionProvider {
 public:
  virtual ~CupsOptionProvider() = default;

  // Returns the supported ipp attributes for the given `option_name`.
  // ipp_attribute_t* is owned by CupsOptionProvider.
  virtual ipp_attribute_t* GetSupportedOptionValues(
      const char* option_name) const = 0;

  // Returns supported attribute values for `option_name` where the value can be
  // converted to a string.
  virtual std::vector<std::string_view> GetSupportedOptionValueStrings(
      const char* option_name) const = 0;

  // Returns the default ipp attributes for the given `option_name`.
  // ipp_attribute_t* is owned by CupsOptionProvider.
  virtual ipp_attribute_t* GetDefaultOptionValue(
      const char* option_name) const = 0;

  // Returns true if the `value` is supported by option `name`.
  virtual bool CheckOptionSupported(const char* name,
                                    const char* value) const = 0;

  // Returns the IPP "media-col-database" attribute for this printer.
  // ipp_attribute_t* is owned by CupsOptionProvider.
  virtual ipp_attribute_t* GetMediaColDatabase() const = 0;

  // Returns the human-readable display name for an option value. Used to get
  // fallback display names for non-standard values that Chromium doesn't have
  // built-in localizations for.
  virtual const char* GetLocalizedOptionValueName(const char* option_name,
                                                  const char* value) const = 0;
};

// Represents a CUPS printer.
// Retrieves information from CUPS printer objects as requested.  This class
// is only valid as long as the CupsConnection which created it exists as they
// share an http connection which the CupsConnection closes on destruction.
class COMPONENT_EXPORT(PRINT_BACKEND) CupsPrinter : public CupsOptionProvider {
 public:
  ~CupsPrinter() override = default;

  // Create a printer with a connection defined by `http` and `dest`.
  static std::unique_ptr<CupsPrinter> Create(http_t* http,
                                             ScopedDestination dest);

  // Returns true if this is the default printer
  virtual bool is_default() const = 0;

  // Returns the name of the printer as configured in CUPS
  virtual std::string GetName() const = 0;

  virtual std::string GetMakeAndModel() const = 0;

  // Returns the "printer-info" option of the printer as configured in CUPS.
  virtual std::string GetInfo() const = 0;

  virtual std::string GetUri() const = 0;

  // Lazily initialize dest info as it can require a network call
  virtual bool EnsureDestInfo() const = 0;

  // Populates `basic_info` with the relevant information about the printer
  virtual bool ToPrinterInfo(PrinterBasicInfo* basic_info) const = 0;

  // Start a print job.  Writes the id of the started job to `job_id`.  `job_id`
  // is 0 if there is an error.  `title` is not sent if empty.  `username` is
  // not sent if empty.  Check availability before using this operation.  Usage
  // on an unavailable printer is undefined.
  virtual ipp_status_t CreateJob(int* job_id,
                                 const std::string& title,
                                 const std::string& username,
                                 ipp_t* attributes) = 0;

  // Add a document to a print job.  `job_id` must be non-zero and refer to a
  // job started with CreateJob.  `docname` will be displayed in print status
  // if not empty.  `last_doc` should be true if this is the last document for
  // this print job.  `username` is not sent if empty.  `attributes` should
  // contain IPP operation and document attributes for the Send-Document
  // operation.
  virtual bool StartDocument(int job_id,
                             const std::string& docname,
                             bool last_doc,
                             const std::string& username,
                             ipp_t* attributes) = 0;

  // Add data to the current document started by StartDocument.  Calling this
  // without a started document will fail.
  virtual bool StreamData(const std::vector<char>& buffer) = 0;

  // Finish the current document.  Another document can be added or the job can
  // be closed to complete printing.
  virtual bool FinishDocument() = 0;

  // Close the job.  If the job is not closed, the documents will not be
  // printed.  `job_id` should match the id from CreateJob.  `username` is not
  // sent if empty.
  virtual ipp_status_t CloseJob(int job_id, const std::string& username) = 0;

  // Cancel the print job `job_id`.  Returns true if the operation succeeded.
  // Returns false if it failed for any reason.
  virtual bool CancelJob(int job_id) = 0;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_PRINTER_H_
