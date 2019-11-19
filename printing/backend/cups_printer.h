// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_PRINTER_H_
#define PRINTING_BACKEND_CUPS_PRINTER_H_

#include <cups/cups.h>

#include <memory>
#include <string>
#include <vector>

#include "printing/backend/cups_deleters.h"
#include "printing/printing_export.h"
#include "url/gurl.h"

namespace printing {

struct PrinterBasicInfo;

// Provides information regarding cups options.
class PRINTING_EXPORT CupsOptionProvider {
 public:
  virtual ~CupsOptionProvider() = default;

  // Returns the supported ipp attributes for the given |option_name|.
  // ipp_attribute_t* is owned by CupsOptionProvider.
  virtual ipp_attribute_t* GetSupportedOptionValues(
      const char* option_name) const = 0;

  // Returns supported attribute values for |option_name| where the value can be
  // convered to a string.
  virtual std::vector<base::StringPiece> GetSupportedOptionValueStrings(
      const char* option_name) const = 0;

  // Returns the default ipp attributes for the given |option_name|.
  // ipp_attribute_t* is owned by CupsOptionProvider.
  virtual ipp_attribute_t* GetDefaultOptionValue(
      const char* option_name) const = 0;

  // Returns true if the |value| is supported by option |name|.
  virtual bool CheckOptionSupported(const char* name,
                                    const char* value) const = 0;
};

// Represents a CUPS printer.
// Retrieves information from CUPS printer objects as requested.  This class
// is only valid as long as the CupsConnection which created it exists as they
// share an http connection which the CupsConnection closes on destruction.
class PRINTING_EXPORT CupsPrinter : public CupsOptionProvider {
 public:
  // Create a printer with a connection defined by |http| and |dest|.
  CupsPrinter(http_t* http, ScopedDestination dest);

  CupsPrinter(CupsPrinter&& printer);

  ~CupsPrinter() override;

  // Returns true if this is the default printer
  bool is_default() const;

  // CupsOptionProvider
  ipp_attribute_t* GetSupportedOptionValues(
      const char* option_name) const override;
  std::vector<base::StringPiece> GetSupportedOptionValueStrings(
      const char* option_name) const override;
  ipp_attribute_t* GetDefaultOptionValue(
      const char* option_name) const override;
  bool CheckOptionSupported(const char* name, const char* value) const override;

  // Returns the contents of the PPD retrieved from the print server.
  std::string GetPPD() const;

  // Returns the name of the printer as configured in CUPS
  std::string GetName() const;

  std::string GetMakeAndModel() const;

  // Lazily initialize dest info as it can require a network call
  bool EnsureDestInfo() const;

  // Populates |basic_info| with the relevant information about the printer
  bool ToPrinterInfo(PrinterBasicInfo* basic_info) const;

  // Start a print job.  Writes the id of the started job to |job_id|.  |job_id|
  // is 0 if there is an error.  |title| is not sent if empty.  |username| is
  // not sent if empty.  Check availability before using this operation.  Usage
  // on an unavailable printer is undefined.
  ipp_status_t CreateJob(int* job_id,
                         const std::string& title,
                         const std::string& username,
                         const std::vector<cups_option_t>& options);

  // Add a document to a print job.  |job_id| must be non-zero and refer to a
  // job started with CreateJob.  |docname| will be displayed in print status
  // if not empty.  |last_doc| should be true if this is the last document for
  // this print job.  |username| is not sent if empty.  |options| should be IPP
  // key value pairs for the Send-Document operation.
  bool StartDocument(int job_id,
                     const std::string& docname,
                     bool last_doc,
                     const std::string& username,
                     const std::vector<cups_option_t>& options);

  // Add data to the current document started by StartDocument.  Calling this
  // without a started document will fail.
  bool StreamData(const std::vector<char>& buffer);

  // Finish the current document.  Another document can be added or the job can
  // be closed to complete printing.
  bool FinishDocument();

  // Close the job.  If the job is not closed, the documents will not be
  // printed.  |job_id| should match the id from CreateJob.  |username| is not
  // sent if empty.
  ipp_status_t CloseJob(int job_id, const std::string& username);

  // Cancel the print job |job_id|.  Returns true if the operation succeeded.
  // Returns false if it failed for any reason.
  bool CancelJob(int job_id);

 private:
  // http connection owned by the CupsConnection which created this object
  http_t* const cups_http_;

  // information to identify a printer
  ScopedDestination destination_;

  // opaque object containing printer attributes and options
  mutable ScopedDestInfo dest_info_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrinter);
};

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_PRINTER_H_
