// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "printing/backend/cups_printer.h"

#include <cups/cups.h>

#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_ipp_helper.h"
#include "printing/backend/cups_weak_functions.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/print_job_constants.h"
#include "url/gurl.h"

namespace printing {

class CupsPrinterImpl : public CupsPrinter {
 public:
  CupsPrinterImpl(http_t* http, ScopedDestination dest)
      : cups_http_(http),
        destination_(std::move(dest)),
        printer_attributes_(WrapIpp(nullptr)) {
    DCHECK(cups_http_);
    DCHECK(destination_);

    const char* printer_uri = cupsGetOption(kCUPSOptPrinterUriSupported,
                                            destination_.get()->num_options,
                                            destination_.get()->options);

    // crbug.com/1418564: Every printer *should* have a "printer-uri-supported"
    // attribute, but make sure Chromium doesn't crash if one doesn't for
    // whatever reason. The printer in question won't actually work, but
    // that's a better outcome than crashing here.
    // TODO(crbug.com/40894807): filter such printers out before reaching this
    // point
    if (printer_uri) {
      printer_uri_ = printer_uri;
      resource_path_ = std::string(GURL(printer_uri_).path_piece());
    }
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
  std::vector<std::string_view> GetSupportedOptionValueStrings(
      const char* option_name) const override {
    std::vector<std::string_view> values;
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

  // CupsOptionProvider
  ipp_attribute_t* GetMediaColDatabase() const override {
    if (!EnsurePrinterAttributes()) {
      return nullptr;
    }

    return ippFindAttribute(printer_attributes_.get(), kIppMediaColDatabase,
                            IPP_TAG_BEGIN_COLLECTION);
  }

  // CupsOptionProvider
  const char* GetLocalizedOptionValueName(const char* option_name,
                                          const char* value) const override {
    if (!EnsureDestInfo()) {
      return nullptr;
    }

    return cupsLocalizeDestValue(cups_http_, destination_.get(),
                                 dest_info_.get(), option_name, value);
  }

  bool ToPrinterInfo(PrinterBasicInfo* printer_info) const override {
    const cups_dest_t* printer = destination_.get();

    printer_info->printer_name = printer->name;
    printer_info->is_default = printer->is_default;

    const std::string info = GetInfo();
    const std::string make_and_model = GetMakeAndModel();

    printer_info->display_name = GetDisplayName(printer->name, info);
    printer_info->printer_description =
        GetPrinterDescription(make_and_model, info);

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
                         ipp_t* attributes) override {
    ScopedIppPtr request = CreateRequest(IPP_OP_CREATE_JOB, username);

    if (!title.empty()) {
      ippAddString(request.get(), IPP_TAG_OPERATION, IPP_TAG_NAME, kIppJobName,
                   nullptr, title.c_str());
    }

    CopyAttributeGroup(request.get(), attributes, IPP_TAG_OPERATION);
    CopyAttributeGroup(request.get(), attributes, IPP_TAG_JOB);
    // We would also copy subscription attributes here if we actually used
    // any. We don't, though.

    // cupsDoRequest() takes ownership of the request and frees it for us.
    ScopedIppPtr response = WrapIpp(
        cupsDoRequest(cups_http_, request.release(), resource_path_.c_str()));

    ipp_attribute_t* attr =
        ippFindAttribute(response.get(), kIppJobId, IPP_TAG_INTEGER);
    *job_id = ippGetInteger(attr, 0);

    return ippGetStatusCode(response.get());
  }

  bool StartDocument(int job_id,
                     const std::string& docname,
                     bool last_document,
                     const std::string& username,
                     ipp_t* attributes) override {
    DCHECK(job_id);
    ScopedIppPtr request = CreateRequest(IPP_OP_SEND_DOCUMENT, username);

    ippAddInteger(request.get(), IPP_TAG_OPERATION, IPP_TAG_INTEGER, kIppJobId,
                  job_id);
    ippAddBoolean(request.get(), IPP_TAG_OPERATION, kIppLastDocument,
                  static_cast<char>(last_document));
    ippAddString(request.get(), IPP_TAG_OPERATION, IPP_TAG_MIMETYPE,
                 kIppDocumentFormat, nullptr, CUPS_FORMAT_PDF);
    if (!docname.empty()) {
      ippAddString(request.get(), IPP_TAG_OPERATION, IPP_TAG_NAME,
                   kIppDocumentName, nullptr, docname.c_str());
    }

    CopyAttributeGroup(request.get(), attributes, IPP_TAG_OPERATION);
    CopyAttributeGroup(request.get(), attributes, IPP_TAG_DOCUMENT);

    http_status_t status =
        cupsSendRequest(cups_http_, request.get(), resource_path_.c_str(),
                        CUPS_LENGTH_VARIABLE);
    return status == HTTP_CONTINUE;
  }

  bool StreamData(const std::vector<char>& buffer) override {
    http_status_t status =
        cupsWriteRequestData(cups_http_, buffer.data(), buffer.size());
    return status == HTTP_STATUS_CONTINUE;
  }

  bool FinishDocument() override {
    ScopedIppPtr response =
        WrapIpp(cupsGetResponse(cups_http_, resource_path_.c_str()));
    ipp_status_t status = ippGetStatusCode(response.get());
    return status == IPP_STATUS_OK;
  }

  ipp_status_t CloseJob(int job_id, const std::string& username) override {
    DCHECK(job_id);
    ScopedIppPtr request = CreateRequest(IPP_OP_CLOSE_JOB, username);

    ippAddInteger(request.get(), IPP_TAG_OPERATION, IPP_TAG_INTEGER, kIppJobId,
                  job_id);

    ScopedIppPtr response = WrapIpp(
        cupsDoRequest(cups_http_, request.release(), resource_path_.c_str()));
    return ippGetStatusCode(response.get());
  }

  bool CancelJob(int job_id) override {
    DCHECK(job_id);

    // TODO(skau): Try to change back to cupsCancelDestJob().
    ipp_status_t status =
        cupsCancelJob2(cups_http_, destination_->name, job_id, 0 /*cancel*/);
    return status == IPP_STATUS_OK;
  }

 private:
  // Sends the request to populate `printer_attributes_` if it's not already
  // populated.
  bool EnsurePrinterAttributes() const {
    if (printer_attributes_) {
      return true;
    }

    ScopedIppPtr request = CreateRequest(IPP_OP_GET_PRINTER_ATTRIBUTES, "");
    // The requested attributes can be changed to "all","media-col-database" if
    // we want to directly query printer attributes other than
    // media-col-database in the future.
    constexpr const char* kRequestedAttributes[] = {kIppMediaColDatabase};
    ippAddStrings(request.get(), IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                  kIppRequestedAttributes, std::size(kRequestedAttributes),
                  nullptr, kRequestedAttributes);

    // cupsDoRequest() takes ownership of the request and frees it for us.
    printer_attributes_.reset(
        cupsDoRequest(cups_http_, request.release(), resource_path_.c_str()));

    if (ippGetStatusCode(printer_attributes_.get()) != IPP_STATUS_OK) {
      printer_attributes_.reset();
      return false;
    }

    // Go through all of our media-col-database entries and consolidate any that
    // have custom size ranges.
    FilterMediaColSizes(printer_attributes_);

    return true;
  }

  // internal helper function to initialize an IPP request
  ScopedIppPtr CreateRequest(ipp_op_t op, const std::string& username) const {
    const char* c_username = username.empty() ? cupsUser() : username.c_str();

    ipp_t* request = ippNewRequest(op);
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, kIppPrinterUri,
                 nullptr, printer_uri_.c_str());
    ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 kIppRequestingUserName, nullptr, c_username);

    return WrapIpp(request);
  }

  // internal helper function to copy attributes to an IPP request
  void CopyAttributeGroup(ipp_t* request,
                          ipp_t* attributes,
                          ipp_tag_t group) const {
    for (ipp_attribute_t* attr = ippFirstAttribute(attributes); attr;
         attr = ippNextAttribute(attributes)) {
      if (ippGetGroupTag(attr) == group) {
        ippCopyAttribute(request, attr, 0);
      }
    }
  }

  // http connection owned by the CupsConnection which created this object
  const raw_ptr<http_t> cups_http_;

  // information to identify a printer
  ScopedDestination destination_;

  // opaque object containing printer attributes and options
  mutable ScopedDestInfo dest_info_;

  // uri used to connect to this printer
  std::string printer_uri_;

  // resource path used to connect to this printer
  std::string resource_path_;

  // printer attributes that describe the supported options
  mutable ScopedIppPtr printer_attributes_;
};

std::unique_ptr<CupsPrinter> CupsPrinter::Create(http_t* http,
                                                 ScopedDestination dest) {
  return std::make_unique<CupsPrinterImpl>(http, std::move(dest));
}

}  // namespace printing
