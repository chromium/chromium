// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PDF_RESOURCE_H_
#define PPAPI_PROXY_PDF_RESOURCE_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/thunk/ppb_pdf_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT PDFResource
    : public PluginResource,
      public thunk::PPB_PDF_API {
 public:
  PDFResource(Connection connection, PP_Instance instance);

  PDFResource(const PDFResource&) = delete;
  PDFResource& operator=(const PDFResource&) = delete;

  ~PDFResource() override;

  // For unittesting with a given locale.
  void SetLocaleForTest(const std::string& locale) {
    locale_ = locale;
  }

  // Resource override.
  thunk::PPB_PDF_API* AsPPB_PDF_API() override;

  // PPB_PDF_API implementation.
  void Print() override;

 private:
  std::string locale_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PDF_RESOURCE_H_
