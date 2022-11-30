// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/trusted/file_chooser_trusted.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/c/trusted/ppb_file_chooser_trusted.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileChooserTrusted_0_5>() {
  return PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5;
}

template <> const char* interface_name<PPB_FileChooserTrusted_0_6>() {
  return PPB_FILECHOOSER_TRUSTED_INTERFACE_0_6;
}

}  // namespace

FileChooser_Trusted::FileChooser_Trusted() : save_as_(false) {
}

FileChooser_Trusted::FileChooser_Trusted(const InstanceHandle& instance,
                                         PP_FileChooserMode_Dev mode,
                                         const Var& accept_types,
                                         bool save_as,
                                         const std::string& suggested_file_name)
    : FileChooser_Dev(instance, mode, accept_types),
      save_as_(save_as),
      suggested_file_name_(suggested_file_name) {
}

FileChooser_Trusted::FileChooser_Trusted(const FileChooser_Trusted& other)
    : FileChooser_Dev(other),
      save_as_(other.save_as_),
      suggested_file_name_(other.suggested_file_name_) {
}

FileChooser_Trusted& FileChooser_Trusted::operator=(
    const FileChooser_Trusted& other) {
  FileChooser_Dev::operator=(other);
  save_as_ = other.save_as_;
  suggested_file_name_ = other.suggested_file_name_;
  return *this;
}

int32_t FileChooser_Trusted::Show(
    const CompletionCallbackWithOutput< std::vector<FileRef> >& callback) {
  if (has_interface<PPB_FileChooserTrusted_0_6>()) {
    return get_interface<PPB_FileChooserTrusted_0_6>()->ShowWithoutUserGesture(
        pp_resource(),
        PP_FromBool(save_as_),
        Var(suggested_file_name_).pp_var(),
        callback.output(),
        callback.pp_completion_callback());
  }
  if (has_interface<PPB_FileChooserTrusted_0_5>()) {
    // Data for our callback. The callback handler will delete it.
    ChooseCallbackData0_5* data = new ChooseCallbackData0_5;
    data->file_chooser = pp_resource();
    data->output = callback.output();
    data->original_callback = callback.pp_completion_callback();

    return get_interface<PPB_FileChooserTrusted_0_5>()->ShowWithoutUserGesture(
        pp_resource(),
        PP_FromBool(save_as_),
        Var(suggested_file_name_).pp_var(),
        PP_MakeCompletionCallback(&CallbackConverter, data));
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
