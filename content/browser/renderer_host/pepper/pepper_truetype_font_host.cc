// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_truetype_font_host.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "content/browser/renderer_host/pepper/pepper_truetype_font.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using ppapi::host::HostMessageContext;
using ppapi::host::ReplyMessageContext;
using ppapi::proxy::SerializedTrueTypeFontDesc;

namespace content {

PepperTrueTypeFontHost::PepperTrueTypeFontHost(
    BrowserPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const SerializedTrueTypeFontDesc& desc)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      initialize_completed_(false) {
  font_ = PepperTrueTypeFont::Create();
  // Initialize the font on a ThreadPool thread. This must complete before
  // using |font_|.
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  SerializedTrueTypeFontDesc* actual_desc =
      new SerializedTrueTypeFontDesc(desc);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&PepperTrueTypeFont::Initialize, font_, actual_desc),
      base::Bind(&PepperTrueTypeFontHost::OnInitializeComplete,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(actual_desc)));
}

PepperTrueTypeFontHost::~PepperTrueTypeFontHost() {
  // Release the font on the task runner in case the implementation requires
  // long blocking operations.
  task_runner_->ReleaseSoon(FROM_HERE, std::move(font_));
}

int32_t PepperTrueTypeFontHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  if (!host()->permissions().HasPermission(ppapi::PERMISSION_DEV))
    return PP_ERROR_FAILED;

  PPAPI_BEGIN_MESSAGE_MAP(PepperTrueTypeFontHost, msg)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_TrueTypeFont_GetTableTags,
                                      OnHostMsgGetTableTags)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_TrueTypeFont_GetTable,
                                    OnHostMsgGetTable)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperTrueTypeFontHost::OnHostMsgGetTableTags(
    HostMessageContext* context) {
  if (!font_.get())
    return PP_ERROR_FAILED;

  // Get font data on a thread that allows slow blocking operations.
  std::vector<uint32_t>* tags = new std::vector<uint32_t>();
  base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&PepperTrueTypeFont::GetTableTags, font_, tags),
      base::Bind(&PepperTrueTypeFontHost::OnGetTableTagsComplete,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(tags),
                 context->MakeReplyMessageContext()));

  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTrueTypeFontHost::OnHostMsgGetTable(HostMessageContext* context,
                                                  uint32_t table,
                                                  int32_t offset,
                                                  int32_t max_data_length) {
  if (!font_.get())
    return PP_ERROR_FAILED;
  if (offset < 0 || max_data_length < 0)
    return PP_ERROR_BADARGUMENT;

  // Get font data on a thread that allows slow blocking operations.
  std::string* data = new std::string();
  base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&PepperTrueTypeFont::GetTable,
                 font_,
                 table,
                 offset,
                 max_data_length,
                 data),
      base::Bind(&PepperTrueTypeFontHost::OnGetTableComplete,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(data),
                 context->MakeReplyMessageContext()));

  return PP_OK_COMPLETIONPENDING;
}

void PepperTrueTypeFontHost::OnInitializeComplete(
    SerializedTrueTypeFontDesc* desc,
    int32_t result) {
  DCHECK(!initialize_completed_);
  initialize_completed_ = true;
  // Release the font if there was an error, so future calls will fail.
  if (result != PP_OK)
    font_ = nullptr;
  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_TrueTypeFont_CreateReply(*desc, result));
}

void PepperTrueTypeFontHost::OnGetTableTagsComplete(
    std::vector<uint32_t>* tags,
    ReplyMessageContext reply_context,
    int32_t result) {
  DCHECK(initialize_completed_);
  // It's possible that Initialize failed and that |font_| is NULL. Check that
  // the font implementation doesn't return PP_OK in that case.
  DCHECK(font_.get() || result != PP_OK);
  reply_context.params.set_result(result);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_TrueTypeFont_GetTableTagsReply(*tags));
}

void PepperTrueTypeFontHost::OnGetTableComplete(
    std::string* data,
    ReplyMessageContext reply_context,
    int32_t result) {
  DCHECK(initialize_completed_);
  // It's possible that Initialize failed and that |font_| is NULL. Check that
  // the font implementation doesn't return PP_OK in that case.
  DCHECK(font_.get() || result != PP_OK);
  reply_context.params.set_result(result);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_TrueTypeFont_GetTableReply(*data));
}

}  // namespace content
