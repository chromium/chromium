// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/pdf_resource.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

PDFResource::PDFResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_PDF_Create());
}

PDFResource::~PDFResource() {
}

thunk::PPB_PDF_API* PDFResource::AsPPB_PDF_API() {
  return this;
}

void PDFResource::Print() {
  Post(RENDERER, PpapiHostMsg_PDF_Print());
}

}  // namespace proxy
}  // namespace ppapi
