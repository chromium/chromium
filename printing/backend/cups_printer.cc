// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_printer.h"

#include <cups/cups.h>

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"

namespace {

const char kCUPSPrinterInfoOpt[] = "printer-info";
const char kCUPSPrinterStateOpt[] = "printer-state";

}  // namespace

namespace printing {

CupsPrinter::CupsPrinter(http_t* http,
                         std::unique_ptr<cups_dest_t, DestinationDeleter> dest,
                         std::unique_ptr<cups_dinfo_t, DestInfoDeleter> info)
    : cups_http_(http),
      destination_(std::move(dest)),
      dest_info_(std::move(info)) {
  DCHECK(cups_http_);
  DCHECK(destination_);
}

CupsPrinter::CupsPrinter(CupsPrinter&& printer)
    : cups_http_(printer.cups_http_),
      destination_(std::move(printer.destination_)),
      dest_info_(std::move(printer.dest_info_)) {
  DCHECK(cups_http_);
  DCHECK(destination_);
}

CupsPrinter::~CupsPrinter() {}

bool CupsPrinter::is_default() const {
  return destination_->is_default;
}

ipp_attribute_t* CupsPrinter::GetSupportedOptionValues(
    const char* option_name) const {
  if (!InitializeDestInfo())
    return nullptr;

  return cupsFindDestSupported(cups_http_, destination_.get(), dest_info_.get(),
                               option_name);
}

std::vector<base::StringPiece> CupsPrinter::GetSupportedOptionValueStrings(
    const char* option_name) const {
  std::vector<base::StringPiece> values;
  ipp_attribute_t* attr = GetSupportedOptionValues(option_name);
  if (!attr)
    return values;

  base::StringPiece value;
  int num_options = ippGetCount(attr);
  for (int i = 0; i < num_options; ++i) {
    value.set(ippGetString(attr, i, nullptr));
    values.push_back(value);
  }

  return values;
}

ipp_attribute_t* CupsPrinter::GetDefaultOptionValue(
    const char* option_name) const {
  if (!InitializeDestInfo())
    return nullptr;

  return cupsFindDestDefault(cups_http_, destination_.get(), dest_info_.get(),
                             option_name);
}

bool CupsPrinter::CheckOptionSupported(const char* name,
                                       const char* value) const {
  if (!InitializeDestInfo())
    return false;

  int supported = cupsCheckDestSupported(cups_http_, destination_.get(),
                                         dest_info_.get(), name, value);
  return supported == 1;
}

bool CupsPrinter::ToPrinterInfo(PrinterBasicInfo* printer_info) const {
  const cups_dest_t* printer = destination_.get();

  printer_info->printer_name = printer->name;
  printer_info->is_default = printer->is_default;

  const char* info = cupsGetOption(kCUPSPrinterInfoOpt, printer->num_options,
                                   printer->options);
  if (info)
    printer_info->printer_description = info;

  const char* state = cupsGetOption(kCUPSPrinterStateOpt, printer->num_options,
                                    printer->options);
  if (state)
    base::StringToInt(state, &printer_info->printer_status);

  const char* drv_info = cupsGetOption(kDriverNameTagName,
                                       printer->num_options, printer->options);
  if (drv_info)
    printer_info->options[kDriverInfoTagName] = *drv_info;

  // Store printer options.
  for (int opt_index = 0; opt_index < printer->num_options; ++opt_index) {
    printer_info->options[printer->options[opt_index].name] =
        printer->options[opt_index].value;
  }

  return true;
}

std::string CupsPrinter::GetName() const {
  return std::string(destination_->name);
}

std::string CupsPrinter::GetMakeAndModel() const {
  const char* make_and_model =
      cupsGetOption(kDriverNameTagName, destination_->num_options,
                    destination_->options);

  return make_and_model ? std::string(make_and_model) : std::string();
}

bool CupsPrinter::IsAvailable() const {
  return InitializeDestInfo();
}

bool CupsPrinter::InitializeDestInfo() const {
  if (dest_info_)
    return true;

  dest_info_.reset(cupsCopyDestInfo(cups_http_, destination_.get()));
  return !!dest_info_;
}

ipp_status_t CupsPrinter::CreateJob(int* job_id,
                                    const std::string& title,
                                    const std::vector<cups_option_t>& options) {
  DCHECK(dest_info_) << "Verify availability before starting a print job";

  cups_option_t* data = const_cast<cups_option_t*>(
      options.data());  // createDestJob will not modify the data
  ipp_status_t create_status =
      cupsCreateDestJob(cups_http_, destination_.get(), dest_info_.get(),
                        job_id, title.c_str(), options.size(), data);

  return create_status;
}

bool CupsPrinter::StartDocument(int job_id,
                                const std::string& document_name,
                                bool last_document,
                                const std::vector<cups_option_t>& options) {
  DCHECK(dest_info_);
  DCHECK(job_id);

  cups_option_t* data = const_cast<cups_option_t*>(
      options.data());  // createStartDestDocument will not modify the data
  http_status_t start_doc_status =
      cupsStartDestDocument(cups_http_, destination_.get(), dest_info_.get(),
                            job_id, document_name.c_str(), CUPS_FORMAT_PDF,
                            options.size(), data, last_document ? 0 : 1);

  return start_doc_status == HTTP_CONTINUE;
}

bool CupsPrinter::StreamData(const std::vector<char>& buffer) {
  http_status_t status =
      cupsWriteRequestData(cups_http_, buffer.data(), buffer.size());
  return status == HTTP_STATUS_CONTINUE;
}

bool CupsPrinter::FinishDocument() {
  DCHECK(dest_info_);

  ipp_status_t status =
      cupsFinishDestDocument(cups_http_, destination_.get(), dest_info_.get());

  return status == IPP_STATUS_OK;
}

ipp_status_t CupsPrinter::CloseJob(int job_id) {
  DCHECK(dest_info_);
  DCHECK(job_id);

  return cupsCloseDestJob(cups_http_, destination_.get(), dest_info_.get(),
                          job_id);
}

bool CupsPrinter::CancelJob(int job_id) {
  DCHECK(job_id);

  // TODO(skau): Try to change back to cupsCancelDestJob().
  ipp_status_t status =
      cupsCancelJob2(cups_http_, destination_->name, job_id, 0 /*cancel*/);
  return status == IPP_STATUS_OK;
}

}  // namespace printing
