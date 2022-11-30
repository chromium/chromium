// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_truetype_font_list_host.h"

#include <stdint.h>

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "content/browser/renderer_host/pepper/pepper_truetype_font_list.h"
#include "content/common/font_list.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

namespace {

// Handles the font list request on the blocking pool.
class TrueTypeFontMessageFilter : public ppapi::host::ResourceMessageFilter {
 public:
  TrueTypeFontMessageFilter();

  TrueTypeFontMessageFilter(const TrueTypeFontMessageFilter&) = delete;
  TrueTypeFontMessageFilter& operator=(const TrueTypeFontMessageFilter&) =
      delete;

  // ppapi::host::ResourceMessageFilter implementation.
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) override;
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  ~TrueTypeFontMessageFilter() override;

  // Message handlers.
  int32_t OnHostMsgGetFontFamilies(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgGetFontsInFamily(ppapi::host::HostMessageContext* context,
                                    const std::string& family);
};

TrueTypeFontMessageFilter::TrueTypeFontMessageFilter() {}

TrueTypeFontMessageFilter::~TrueTypeFontMessageFilter() {}

scoped_refptr<base::SequencedTaskRunner>
TrueTypeFontMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  // Use the font list SequencedTaskRunner to get the font list (currently the
  // only message) since getting the font list is non-threadsafe on Linux (for
  // versions of Pango predating 2013).
  return GetFontListTaskRunner();
}

int32_t TrueTypeFontMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(TrueTypeFontMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_TrueTypeFontSingleton_GetFontFamilies,
        OnHostMsgGetFontFamilies)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TrueTypeFontSingleton_GetFontsInFamily,
        OnHostMsgGetFontsInFamily)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t TrueTypeFontMessageFilter::OnHostMsgGetFontFamilies(
    ppapi::host::HostMessageContext* context) {
  // OK to use "slow blocking" version since we're on the blocking pool.
  std::vector<std::string> font_families;
  GetFontFamilies_SlowBlocking(&font_families);
  // Sort the names in case the host platform returns them out of order.
  std::sort(font_families.begin(), font_families.end());

  context->reply_msg =
      PpapiPluginMsg_TrueTypeFontSingleton_GetFontFamiliesReply(font_families);
  return base::checked_cast<int32_t>(font_families.size());
}

int32_t TrueTypeFontMessageFilter::OnHostMsgGetFontsInFamily(
    ppapi::host::HostMessageContext* context,
    const std::string& family) {
  // OK to use "slow blocking" version since we're on the blocking pool.
  std::vector<ppapi::proxy::SerializedTrueTypeFontDesc> fonts_in_family;
  GetFontsInFamily_SlowBlocking(family, &fonts_in_family);

  context->reply_msg =
      PpapiPluginMsg_TrueTypeFontSingleton_GetFontsInFamilyReply(
          fonts_in_family);
  return base::checked_cast<int32_t>(fonts_in_family.size());
}

}  // namespace

PepperTrueTypeFontListHost::PepperTrueTypeFontListHost(BrowserPpapiHost* host,
                                                       PP_Instance instance,
                                                       PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
  AddFilter(scoped_refptr<ppapi::host::ResourceMessageFilter>(
      new TrueTypeFontMessageFilter()));
}

PepperTrueTypeFontListHost::~PepperTrueTypeFontListHost() {}

}  // namespace content
