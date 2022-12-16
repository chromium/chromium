// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_printer.h"

#include <cups/cups.h>

#include <cstring>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/print_job_constants.h"

namespace printing {

class CupsPrinterImpl : public CupsPrinter {
 public:
  CupsPrinterImpl(http_t* http, ScopedDestination dest)
      : cups_http_(http), destination_(std::move(dest)) {
    DCHECK(cups_http_);
    DCHECK(destination_);
  }

  CupsPrinterImpl(const CupsPrinterImpl&) = delete;
  CupsPrinterImpl& operator=(const CupsPrinterImpl&) = delete;

  ~CupsPrinterImpl() override = default;

  bool is_default() const override { return destination_->is_default; }

  // CupsOptionProvider
  ipp_attribute_t* GetSupportedOptionValues(
      const char* option_name) const override {
    if (!EnsureDestInfo())
      return nullptr;

    return cupsFindDestSupported(cups_http_, destination_.get(),
                                 dest_info_.get(), option_name);
  }

  // CupsOptionProvider
  std::vector<base::StringPiece> GetSupportedOptionValueStrings(
      const char* option_name) const override {
    std::vector<base::StringPiece> values;
    ipp_attribute_t* attr = GetSupportedOptionValues(option_name);
    if (!attr)
      return values;

    int num_options = ippGetCount(attr);
    for (int i = 0; i < num_options; ++i) {
      const char* const value = ippGetString(attr, i, nullptr);
      if (!value) {
        continue;
      }
      values.push_back(value);
    }

    return values;
  }

  // CupsOptionProvider
  ipp_attribute_t* GetDefaultOptionValue(
      const char* option_name) const override {
    if (!EnsureDestInfo())
      return nullptr;

    return cupsFindDestDefault(cups_http_, destination_.get(), dest_info_.get(),
                               option_name);
  }

  // CupsOptionProvider
  bool CheckOptionSupported(const char* name,
                            const char* value) const override {
    if (!EnsureDestInfo())
      return false;

#if BUILDFLAG(IS_CHROMEOS)
    // OAuth token passed to CUPS as IPP attribute, see b/200086039.
    if (name && strcmp(name, kSettingChromeOSAccessOAuthToken) == 0)
      return true;

    // Special case for the IPP 'client-info' collection because
    // cupsCheckDestSupported will not report it as supported even when it is.
    // See http://b/238761330.
    if (name && strcmp(name, kIppClientInfo) == 0) {
      return true;
    }
#endif

    int supported = cupsCheckDestSupported(cups_http_, destination_.get(),
                                           dest_info_.get(), name, value);
    return supported == 1;
  }

  bool ToPrinterInfo(PrinterBasicInfo* printer_info) const override {
    const cups_dest_t* printer = destination_.get();

    printer_info->printer_name = printer->name;
    printer_info->is_default = printer->is_default;

    const std::string info = GetInfo();
    const std::string make_and_model = GetMakeAndModel();

#if BUILDFLAG(IS_MAC)
    // On Mac, "printer-info" option specifies the human-readable printer name,
    // while "printer-make-and-model" specifies the printer description.
    printer_info->display_name = info;
    printer_info->printer_description = make_and_model;
#else
    // On other platforms, "printer-info" specifies the printer description.
    printer_info->display_name = printer->name;
    printer_info->printer_description = info;
#endif  // BUILDFLAG(IS_MAC)

    const char* state = cupsGetOption(kCUPSOptPrinterState,
                                      printer->num_options, printer->options);
    if (state)
      base::StringToInt(state, &printer_info->printer_status);

    printer_info->options[kDriverInfoTagName] = make_and_model;

    // Store printer options.
    for (int opt_index = 0; opt_index < printer->num_options; ++opt_index) {
      printer_info->options[printer->options[opt_index].name] =
          printer->options[opt_index].value;
    }

    return true;
  }

  std::string GetName() const override {
    return std::string(destination_->name);
  }

  std::string GetMakeAndModel() const override {
    const char* make_and_model =
        cupsGetOption(kCUPSOptPrinterMakeAndModel, destination_->num_options,
                      destination_->options);

    return make_and_model ? std::string(make_and_model) : std::string();
  }

  std::string GetInfo() const override {
    const char* info = cupsGetOption(
        kCUPSOptPrinterInfo, destination_->num_options, destination_->options);

    return info ? std::string(info) : std::string();
  }

  std::string GetUri() const override {
    const char* uri = cupsGetOption(
        kCUPSOptDeviceUri, destination_->num_options, destination_->options);
    return uri ? std::string(uri) : std::string();
  }

  bool EnsureDestInfo() const override {
    if (dest_info_)
      return true;

    dest_info_.reset(cupsCopyDestInfo(cups_http_, destination_.get()));
    return !!dest_info_;
  }

  ipp_status_t CreateJob(int* job_id,
                         const std::string& title,
                         const std::string& username,
                         const std::vector<cups_option_t>& options) override {
    DCHECK(dest_info_) << "Verify availability before starting a print job";

    cups_option_t* data = const_cast<cups_option_t*>(
        options.data());  // createDestJob will not modify the data
    if (!username.empty())
      cupsSetUser(username.c_str());

    ipp_status_t create_status = cupsCreateDestJob(
        cups_http_, destination_.get(), dest_info_.get(), job_id,
        title.empty() ? nullptr : title.c_str(), options.size(), data);
    cupsSetUser(nullptr);  // reset to default username ("anonymous")
    return create_status;
  }

  bool StartDocument(int job_id,
                     const std::string& docname,
                     bool last_document,
                     const std::string& username,
                     const std::vector<cups_option_t>& options) override {
    DCHECK(dest_info_);
    DCHECK(job_id);
    if (!username.empty())
      cupsSetUser(username.c_str());

    cups_option_t* data = const_cast<cups_option_t*>(
        options.data());  // createStartDestDocument will not modify the data
    http_status_t start_doc_status = cupsStartDestDocument(
        cups_http_, destination_.get(), dest_info_.get(), job_id,
        docname.empty() ? nullptr : docname.c_str(), CUPS_FORMAT_PDF,
        options.size(), data, last_document ? 1 : 0);

    cupsSetUser(nullptr);  // reset to default username ("anonymous")
    return start_doc_status == HTTP_CONTINUE;
  }

  bool StreamData(const std::vector<char>& buffer) override {
    http_status_t status =
        cupsWriteRequestData(cups_http_, buffer.data(), buffer.size());
    return status == HTTP_STATUS_CONTINUE;
  }

  bool FinishDocument() override {
    DCHECK(dest_info_);

    ipp_status_t status = cupsFinishDestDocument(cups_http_, destination_.get(),
                                                 dest_info_.get());

    return status == IPP_STATUS_OK;
  }

  ipp_status_t CloseJob(int job_id, const std::string& username) override {
    DCHECK(dest_info_);
    DCHECK(job_id);
    if (!username.empty())
      cupsSetUser(username.c_str());

    ipp_status_t result = cupsCloseDestJob(cups_http_, destination_.get(),
                                           dest_info_.get(), job_id);
    cupsSetUser(nullptr);  // reset to default username ("anonymous")
    return result;
  }

  bool CancelJob(int job_id) override {
    DCHECK(job_id);

    // TODO(skau): Try to change back to cupsCancelDestJob().
    ipp_status_t status =
        cupsCancelJob2(cups_http_, destination_->name, job_id, 0 /*cancel*/);
    return status == IPP_STATUS_OK;
  }

  CupsMediaMargins GetMediaMarginsByName(const std::string& media_id) override {
    cups_size_t cups_media;
    if (!EnsureDestInfo() ||
        !cupsGetDestMediaByName(cups_http_, destination_.get(),
                                dest_info_.get(), media_id.c_str(),
                                CUPS_MEDIA_FLAGS_DEFAULT, &cups_media)) {
      return {0, 0, 0, 0};
    }
    return {cups_media.bottom, cups_media.left, cups_media.right,
            cups_media.top};
  }

 private:
  // http connection owned by the CupsConnection which created this object
  const raw_ptr<http_t> cups_http_;

  // information to identify a printer
  ScopedDestination destination_;

  // opaque object containing printer attributes and options
  mutable ScopedDestInfo dest_info_;
};

std::unique_ptr<CupsPrinter> CupsPrinter::Create(http_t* http,
                                                 ScopedDestination dest) {
  return std::make_unique<CupsPrinterImpl>(http, std::move(dest));
}

}  // namespace printing
