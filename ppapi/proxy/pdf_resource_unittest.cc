// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/strings/utf_string_conversions.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/proxy/pdf_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/ppb_image_data_proxy.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef PluginProxyTest PDFResourceTest;

}  // namespace

TEST_F(PDFResourceTest, SearchString) {
  ProxyAutoLock lock;
  // Instantiate a resource explicitly so we can specify the locale.
  auto pdf_resource = base::MakeRefCounted<PDFResource>(
      Connection(&sink(), &sink(), 0), pp_instance());
  pdf_resource->SetLocaleForTest("en-US@collation=search");

  base::string16 input;
  base::string16 term;
  base::UTF8ToUTF16("abcdefabcdef", 12, &input);
  base::UTF8ToUTF16("bc", 2, &term);

  PP_PrivateFindResult* results;
  uint32_t count = 0;
  pdf_resource->SearchString(
      reinterpret_cast<const unsigned short*>(input.c_str()),
      reinterpret_cast<const unsigned short*>(term.c_str()),
      true,
      &results,
      &count);

  ASSERT_EQ(2U, count);
  ASSERT_EQ(1, results[0].start_index);
  ASSERT_EQ(2, results[0].length);
  ASSERT_EQ(7, results[1].start_index);
  ASSERT_EQ(2, results[1].length);

  const PPB_Memory_Dev* memory_iface = thunk::GetPPB_Memory_Dev_0_1_Thunk();
  memory_iface->MemFree(results);
}

TEST_F(PDFResourceTest, DidStartLoading) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->DidStartLoading(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_DidStartLoading::ID, &params, &msg));
}

TEST_F(PDFResourceTest, DidStopLoading) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->DidStopLoading(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_DidStopLoading::ID, &params, &msg));
}

TEST_F(PDFResourceTest, SetContentRestriction) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  int restrictions = 5;
  pdf_iface->SetContentRestriction(pp_instance(), restrictions);

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_SetContentRestriction::ID, &params, &msg));
}

TEST_F(PDFResourceTest, HasUnsupportedFeature) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->HasUnsupportedFeature(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_HasUnsupportedFeature::ID, &params, &msg));
}

TEST_F(PDFResourceTest, Print) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->Print(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_Print::ID, &params, &msg));
}

TEST_F(PDFResourceTest, SaveAs) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  pdf_iface->SaveAs(pp_instance());

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_SaveAs::ID, &params, &msg));
}

TEST_F(PDFResourceTest, SelectionChanged) {
  const PPB_PDF* pdf_iface = thunk::GetPPB_PDF_Thunk();

  PP_FloatPoint left = PP_MakeFloatPoint(0.0f, 0.0f);
  PP_FloatPoint right = PP_MakeFloatPoint(1.0f, 1.0f);
  pdf_iface->SelectionChanged(pp_instance(), &left, 0, &right, 0);

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_PDF_SelectionChanged::ID, &params, &msg));
}

}  // namespace proxy
}  // namespace ppapi
