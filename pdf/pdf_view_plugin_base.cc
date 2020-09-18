// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/ppapi_migration/url_loader.h"

namespace chrome_pdf {

PdfViewPluginBase::PdfViewPluginBase() = default;

PdfViewPluginBase::~PdfViewPluginBase() = default;

void PdfViewPluginBase::InitializeEngine(
    PDFiumFormFiller::ScriptOption script_option) {
  engine_ = std::make_unique<PDFiumEngine>(this, script_option);
}

void PdfViewPluginBase::DestroyEngine() {
  engine_.reset();
}

void PdfViewPluginBase::LoadUrl(const std::string& url, bool is_print_preview) {
  UrlRequest request;
  request.url = url;
  request.method = "GET";
  request.ignore_redirects = true;

  std::unique_ptr<UrlLoader> loader = CreateUrlLoaderInternal();
  UrlLoader* raw_loader = loader.get();
  raw_loader->Open(
      request,
      base::BindOnce(is_print_preview ? &PdfViewPluginBase::DidOpenPreview
                                      : &PdfViewPluginBase::DidOpen,
                     GetWeakPtr(), std::move(loader)));
}

}  // namespace chrome_pdf
