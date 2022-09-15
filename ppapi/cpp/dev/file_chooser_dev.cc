// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_chooser_dev.h"

#include <stddef.h>
#include <string.h>

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileChooser_Dev_0_5>() {
  return PPB_FILECHOOSER_DEV_INTERFACE_0_5;
}

template <> const char* interface_name<PPB_FileChooser_Dev_0_6>() {
  return PPB_FILECHOOSER_DEV_INTERFACE_0_6;
}

}  // namespace

FileChooser_Dev::FileChooser_Dev(const InstanceHandle& instance,
                                 PP_FileChooserMode_Dev mode,
                                 const Var& accept_types) {
  if (has_interface<PPB_FileChooser_Dev_0_6>()) {
    PassRefFromConstructor(get_interface<PPB_FileChooser_Dev_0_6>()->Create(
        instance.pp_instance(), mode, accept_types.pp_var()));
  } else if (has_interface<PPB_FileChooser_Dev_0_5>()) {
    PassRefFromConstructor(get_interface<PPB_FileChooser_Dev_0_5>()->Create(
        instance.pp_instance(), mode, accept_types.pp_var()));
  }
}

FileChooser_Dev::FileChooser_Dev(const FileChooser_Dev& other)
    : Resource(other) {}

FileChooser_Dev& FileChooser_Dev::operator=(const FileChooser_Dev& other) {
  Resource::operator=(other);
  return *this;
}

int32_t FileChooser_Dev::Show(
    const CompletionCallbackWithOutput< std::vector<FileRef> >& callback) {
  if (has_interface<PPB_FileChooser_Dev_0_6>()) {
    return get_interface<PPB_FileChooser_Dev_0_6>()->Show(
        pp_resource(),
        callback.output(),
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_FileChooser_Dev_0_5>()) {
    // Data for our callback wrapper. The callback handler will delete it.
    ChooseCallbackData0_5* data = new ChooseCallbackData0_5;
    data->file_chooser = pp_resource();
    data->output = callback.output();
    data->original_callback = callback.pp_completion_callback();

    return get_interface<PPB_FileChooser_Dev_0_5>()->Show(
        pp_resource(), PP_MakeCompletionCallback(&CallbackConverter, data));
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

// static
void FileChooser_Dev::CallbackConverter(void* user_data, int32_t result) {
  ChooseCallbackData0_5* data = static_cast<ChooseCallbackData0_5*>(user_data);

  // Get all of the selected file resources using the iterator API.
  std::vector<PP_Resource> selected_files;
  if (result == PP_OK) {
    const PPB_FileChooser_Dev_0_5* chooser =
        get_interface<PPB_FileChooser_Dev_0_5>();
    while (PP_Resource cur = chooser->GetNextChosenFile(data->file_chooser))
      selected_files.push_back(cur);
  }

  // Need to issue the "GetDataBuffer" even for error cases & when the
  // number of items is 0.
  void* output_buf = data->output.GetDataBuffer(
      data->output.user_data,
      static_cast<uint32_t>(selected_files.size()), sizeof(PP_Resource));
  if (output_buf) {
    if (!selected_files.empty()) {
      memcpy(output_buf, &selected_files[0],
             sizeof(PP_Resource) * selected_files.size());
    }
  } else {
    // Error allocating, need to free the resource IDs.
    for (size_t i = 0; i < selected_files.size(); i++)
      Module::Get()->core()->ReleaseResource(selected_files[i]);
  }

  // Now execute the original callback.
  PP_RunCompletionCallback(&data->original_callback, result);
  delete data;
}

}  // namespace pp
