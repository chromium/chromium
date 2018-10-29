// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_chooser_host.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_file_chooser_completion.h"

namespace content {

using blink::mojom::FileChooserParams;

class PepperFileChooserHost::CompletionHandler
    : public blink::WebFileChooserCompletion {
 public:
  explicit CompletionHandler(const base::WeakPtr<PepperFileChooserHost>& host)
      : host_(host) {}

  ~CompletionHandler() override {}

  void DidChooseFile(
      const blink::WebVector<blink::WebString>& file_names) override {
    if (host_.get()) {
      std::vector<PepperFileChooserHost::ChosenFileInfo> files;
      for (size_t i = 0; i < file_names.size(); i++) {
        files.push_back(PepperFileChooserHost::ChosenFileInfo(
            blink::WebStringToFilePath(file_names[i]), std::string()));
      }
      host_->StoreChosenFiles(files);
    }

    // It is the responsibility of this method to delete the instance.
    delete this;
  }
  void DidChooseFile(
      const blink::WebVector<SelectedFileInfo>& file_names) override {
    if (host_.get()) {
      std::vector<PepperFileChooserHost::ChosenFileInfo> files;
      for (size_t i = 0; i < file_names.size(); i++) {
        files.push_back(PepperFileChooserHost::ChosenFileInfo(
            file_names[i].file_path, file_names[i].display_name.Utf8()));
      }
      host_->StoreChosenFiles(files);
    }

    // It is the responsibility of this method to delete the instance.
    delete this;
  }

 private:
  base::WeakPtr<PepperFileChooserHost> host_;

  DISALLOW_COPY_AND_ASSIGN(CompletionHandler);
};

PepperFileChooserHost::ChosenFileInfo::ChosenFileInfo(
    const base::FilePath& file_path,
    const std::string& display_name)
    : file_path(file_path), display_name(display_name) {}

PepperFileChooserHost::PepperFileChooserHost(RendererPpapiHost* host,
                                             PP_Instance instance,
                                             PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      handler_(nullptr),
      weak_factory_(this) {}

PepperFileChooserHost::~PepperFileChooserHost() {}

int32_t PepperFileChooserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFileChooserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileChooser_Show, OnShow)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperFileChooserHost::StoreChosenFiles(
    const std::vector<ChosenFileInfo>& files) {
  std::vector<IPC::Message> create_msgs;
  std::vector<base::FilePath> file_paths;
  std::vector<std::string> display_names;
  for (size_t i = 0; i < files.size(); i++) {
    base::FilePath file_path = files[i].file_path;
    file_paths.push_back(file_path);
    create_msgs.push_back(PpapiHostMsg_FileRef_CreateForRawFS(file_path));
    display_names.push_back(files[i].display_name);
  }

  if (!files.empty()) {
    renderer_ppapi_host_->CreateBrowserResourceHosts(
        pp_instance(),
        create_msgs,
        base::Bind(&PepperFileChooserHost::DidCreateResourceHosts,
                   weak_factory_.GetWeakPtr(),
                   file_paths,
                   display_names));
  } else {
    reply_context_.params.set_result(PP_ERROR_USERCANCEL);
    std::vector<ppapi::FileRefCreateInfo> chosen_files;
    host()->SendReply(reply_context_,
                      PpapiPluginMsg_FileChooser_ShowReply(chosen_files));
    reply_context_ = ppapi::host::ReplyMessageContext();
    handler_ = nullptr;  // Handler deletes itself.
  }
}

int32_t PepperFileChooserHost::OnShow(
    ppapi::host::HostMessageContext* context,
    bool save_as,
    bool open_multiple,
    const std::string& suggested_file_name,
    const std::vector<std::string>& accept_mime_types) {
  if (handler_)
    return PP_ERROR_INPROGRESS;  // Already pending.

  if (!host()->permissions().HasPermission(
          ppapi::PERMISSION_BYPASS_USER_GESTURE) &&
      !renderer_ppapi_host_->HasUserGesture(pp_instance())) {
    return PP_ERROR_NO_USER_GESTURE;
  }

  FileChooserParams params;
  if (save_as) {
    params.mode = FileChooserParams::Mode::kSave;
    params.default_file_name =
        base::FilePath::FromUTF8Unsafe(suggested_file_name).BaseName();
  } else {
    params.mode = open_multiple ? FileChooserParams::Mode::kOpenMultiple
                                : FileChooserParams::Mode::kOpen;
  }
  params.accept_types.reserve(accept_mime_types.size());
  for (const auto& mime_type : accept_mime_types)
    params.accept_types.push_back(base::UTF8ToUTF16(mime_type));
  params.need_local_path = true;

  params.requestor = renderer_ppapi_host_->GetDocumentURL(pp_instance());

  handler_ = new CompletionHandler(AsWeakPtr());
  RenderFrameImpl* render_frame = static_cast<RenderFrameImpl*>(
      renderer_ppapi_host_->GetRenderFrameForInstance(pp_instance()));

  if (!render_frame || !render_frame->RunFileChooser(params, handler_)) {
    delete handler_;
    handler_ = nullptr;
    return PP_ERROR_NOACCESS;
  }

  reply_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileChooserHost::DidCreateResourceHosts(
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& display_names,
    const std::vector<int>& browser_ids) {
  DCHECK(file_paths.size() == display_names.size());
  DCHECK(file_paths.size() == browser_ids.size());

  std::vector<ppapi::FileRefCreateInfo> chosen_files;
  for (size_t i = 0; i < browser_ids.size(); ++i) {
    PepperFileRefRendererHost* renderer_host = new PepperFileRefRendererHost(
        renderer_ppapi_host_, pp_instance(), 0, file_paths[i]);
    int renderer_id =
        renderer_ppapi_host_->GetPpapiHost()->AddPendingResourceHost(
            std::unique_ptr<ppapi::host::ResourceHost>(renderer_host));
    ppapi::FileRefCreateInfo info = ppapi::MakeExternalFileRefCreateInfo(
        file_paths[i], display_names[i], browser_ids[i], renderer_id);
    chosen_files.push_back(info);
  }

  reply_context_.params.set_result(PP_OK);
  host()->SendReply(reply_context_,
                    PpapiPluginMsg_FileChooser_ShowReply(chosen_files));
  reply_context_ = ppapi::host::ReplyMessageContext();
  handler_ = nullptr;  // Handler deletes itself.
}

}  // namespace content
