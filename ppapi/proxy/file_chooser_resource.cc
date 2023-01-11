// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_chooser_resource.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/file_ref_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

FileChooserResource::FileChooserResource(Connection connection,
                                         PP_Instance instance,
                                         PP_FileChooserMode_Dev mode,
                                         const std::string& accept_types)
    : PluginResource(connection, instance),
      mode_(mode) {
  PopulateAcceptTypes(accept_types, &accept_types_);
}

FileChooserResource::~FileChooserResource() {
}

thunk::PPB_FileChooser_API* FileChooserResource::AsPPB_FileChooser_API() {
  return this;
}

int32_t FileChooserResource::Show(const PP_ArrayOutput& output,
                                  scoped_refptr<TrackedCallback> callback) {
  return ShowWithoutUserGesture(PP_FALSE, PP_MakeUndefined(), output, callback);
}

int32_t FileChooserResource::ShowWithoutUserGesture(
    PP_Bool save_as,
    PP_Var suggested_file_name,
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  int32_t result = ShowInternal(save_as, suggested_file_name, callback);
  if (result == PP_OK_COMPLETIONPENDING)
    output_.set_pp_array_output(output);
  return result;
}

int32_t FileChooserResource::Show0_5(scoped_refptr<TrackedCallback> callback) {
  return ShowInternal(PP_FALSE, PP_MakeUndefined(), callback);
}

PP_Resource FileChooserResource::GetNextChosenFile() {
 if (file_queue_.empty())
    return 0;

  // Return the next resource in the queue. It will already have been addrefed
  // (they're currently owned by the FileChooser) and returning it transfers
  // ownership of that reference to the plugin.
  PP_Resource next = file_queue_.front();
  file_queue_.pop();
  return next;
}

int32_t FileChooserResource::ShowWithoutUserGesture0_5(
    PP_Bool save_as,
    PP_Var suggested_file_name,
    scoped_refptr<TrackedCallback> callback) {
  return ShowInternal(save_as, suggested_file_name, callback);
}

// static
void FileChooserResource::PopulateAcceptTypes(
    const std::string& input,
    std::vector<std::string>* output) {
  if (input.empty())
    return;

  std::vector<std::string> type_list = base::SplitString(
      input, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  output->reserve(type_list.size());

  for (size_t i = 0; i < type_list.size(); ++i) {
    std::string type = type_list[i];
    base::TrimWhitespaceASCII(type, base::TRIM_ALL, &type);

    // If the type is a single character, it definitely cannot be valid. In the
    // case of a file extension it would be a single ".". In the case of a MIME
    // type it would just be a "/".
    if (type.length() < 2)
      continue;
    if (type.find_first_of('/') == std::string::npos && type[0] != '.')
      continue;
    output->push_back(base::ToLowerASCII(type));
  }
}

void FileChooserResource::OnPluginMsgShowReply(
    const ResourceMessageReplyParams& params,
    const std::vector<FileRefCreateInfo>& chosen_files) {
  if (output_.is_valid()) {
    // Using v0.6 of the API with the output array.
    std::vector<PP_Resource> files;
    for (size_t i = 0; i < chosen_files.size(); i++) {
      files.push_back(FileRefResource::CreateFileRef(
          connection(),
          pp_instance(),
          chosen_files[i]));
    }
    output_.StoreResourceVector(files);
  } else {
    // Convert each of the passed in file infos to resources. These will be
    // owned by the FileChooser object until they're passed to the plugin.
    DCHECK(file_queue_.empty());
    for (size_t i = 0; i < chosen_files.size(); i++) {
      file_queue_.push(FileRefResource::CreateFileRef(
          connection(),
          pp_instance(),
          chosen_files[i]));
    }
  }

  // Notify the plugin of the new data.
  callback_->Run(params.result());
  // DANGER: May delete |this|!
}

int32_t FileChooserResource::ShowInternal(
    PP_Bool save_as,
    const PP_Var& suggested_file_name,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(callback_))
    return PP_ERROR_INPROGRESS;

  if (!sent_create_to_renderer())
    SendCreate(RENDERER, PpapiHostMsg_FileChooser_Create());

  callback_ = callback;
  StringVar* sugg_str = StringVar::FromPPVar(suggested_file_name);

  PpapiHostMsg_FileChooser_Show msg(
        PP_ToBool(save_as),
        mode_ == PP_FILECHOOSERMODE_OPENMULTIPLE,
        sugg_str ? sugg_str->value() : std::string(),
        accept_types_);
  Call<PpapiPluginMsg_FileChooser_ShowReply>(
      RENDERER, msg,
      base::BindOnce(&FileChooserResource::OnPluginMsgShowReply, this));
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace proxy
}  // namespace ppapi
