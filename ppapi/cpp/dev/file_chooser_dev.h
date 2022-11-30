// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_
#define PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_

#include <stdint.h>

#include <vector>

#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class FileRef;
class InstanceHandle;
class Var;

class FileChooser_Dev : public Resource {
 public:
  /// Creates an is_null() FileChooser object.
  FileChooser_Dev() {}

  /// This function creates a file chooser dialog resource.  The chooser is
  /// associated with a particular instance, so that it may be positioned on the
  /// screen relative to the tab containing the instance.  Returns 0 if passed
  /// an invalid instance.
  ///
  /// @param mode A PPB_FileChooser_Dev instance can be used to select a single
  /// file (PP_FILECHOOSERMODE_OPEN) or multiple files
  /// (PP_FILECHOOSERMODE_OPENMULTIPLE). Unlike the HTML5 <input type="file">
  /// tag, a PPB_FileChooser_Dev instance cannot be used to select a directory.
  /// In order to get the list of files in a directory, the
  /// PPB_FileRef::ReadDirectoryEntries interface must be used.
  ///
  /// @param accept_types A comma-separated list of MIME types and file
  /// extensions such as "audio/ *,text/plain,.html" (note there should be
  /// no space between the '/' and the '*', but one is added to avoid confusing
  /// C++ comments). The dialog may restrict selectable files to the specified
  /// MIME types and file extensions. If a string in the comma-separated list
  /// begins with a period (.) then the string is interpreted as a file
  /// extension, otherwise it is interpreted as a MIME-type. An empty string or
  /// an undefined var may be given to indicate that all types should be
  /// accepted.
  FileChooser_Dev(const InstanceHandle& instance,
                  PP_FileChooserMode_Dev mode,
                  const Var& accept_types);

  FileChooser_Dev(const FileChooser_Dev& other);
  FileChooser_Dev& operator=(const FileChooser_Dev& other);

  /// This function displays a previously created file chooser resource as a
  /// dialog box, prompting the user to choose a file or files. This function
  /// must be called in response to a user gesture, such as a mouse click or
  /// touch event. The callback is called with PP_OK on successful completion
  /// with a file (or files) selected, PP_ERROR_USERCANCEL if the user selected
  /// no file, or another error code from pp_errors.h on failure.
  ///
  /// @param callback The completion callback that will be executed. On success,
  /// the selected files will be passed to the given function.
  ///
  /// Normally you would use a CompletionCallbackFactory to allow callbacks to
  /// be bound to your class. See completion_callback_factory.h for more
  /// discussion on how to use this. Your callback will generally look like:
  ///
  /// @code
  ///   void OnFilesSelected(int32_t result,
  ///                        const std::vector<pp::FileRef>& files) {
  ///     if (result == PP_OK)
  ///       // use files...
  ///   }
  /// @endcode
  ///
  /// @return PP_OK_COMPLETIONPENDING if request to show the dialog was
  /// successful, another error code from pp_errors.h on failure.
  virtual int32_t Show(
      const CompletionCallbackWithOutput< std::vector<FileRef> >& callback);

 protected:
  // Heap-allocated data passed to the CallbackConverter for backwards compat.
  struct ChooseCallbackData0_5 {
    PP_Resource file_chooser;
    PP_ArrayOutput output;
    PP_CompletionCallback original_callback;
  };

  // Provide backwards-compatibility for older versions. Converts the old-style
  // 0.5 "iterator" interface to the new-style 0.6 "array output" interface that
  // the caller is expecting.
  //
  // This takes a heap-allocated ChooseCallbackData0_5 struct passed as the
  // user data and deletes it when the call completes.
  static void CallbackConverter(void* user_data, int32_t result);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_
