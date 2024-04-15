// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstdlib>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/timer/timer.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "remoting/host/file_transfer/file_chooser.h"
#include "remoting/host/file_transfer/file_chooser_common_win.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

namespace {

// Converts an HRESULT to a FileTransferResult.
protocol::FileTransfer_Error LogFailedHrAndMakeError(base::Location from_here,
                                                     const char* operation,
                                                     HRESULT hr) {
  DCHECK(FAILED(hr));
  bool canceled = (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED));
  if (!canceled) {
    LOG(ERROR) << "Error displaying file dialog (" << operation << "): " << hr;
  }
  return protocol::MakeFileTransferError(
      from_here,
      canceled ? protocol::FileTransfer_Error_Type_CANCELED
               : protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
      hr);
}

// Loads an embedded string resource from the specified module.
protocol::FileTransferResult<std::wstring> LoadStringResource(int resource_id) {
  // GetModuleHandle doesn't increment the ref count, so the handle doesn't need
  // to be freed.
  HMODULE resource_module = GetModuleHandle(L"remoting_core.dll");

  if (resource_module == nullptr) {
    PLOG(ERROR) << "GetModuleHandle() failed";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError());
  }

  const wchar_t* string_resource = nullptr;
  // Specifying 0 for the last parameter (buffer size) signifies that we want a
  // read-only pointer to the resource instead of copying the string into a
  // buffer (which we do ourselves).
  int string_length =
      LoadString(resource_module, resource_id,
                 reinterpret_cast<wchar_t*>(&string_resource), 0);
  if (string_length <= 0) {
    PLOG(ERROR) << "LoadString() failed";
    return protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR,
        GetLastError());
  }

  return std::wstring(string_resource, string_length);
}

FileChooser::Result ShowFileChooser() {
  HRESULT hr;
  Microsoft::WRL::ComPtr<IFileOpenDialog> file_open_dialog;

  hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&file_open_dialog));
  if (FAILED(hr)) {
    return LogFailedHrAndMakeError(FROM_HERE, "create", hr);
  }

  protocol::FileTransferResult<std::wstring> title =
      LoadStringResource(IDS_DOWNLOAD_FILE_DIALOG_TITLE);
  if (!title) {
    return title.error();
  }

  hr = file_open_dialog->SetTitle(title->c_str());
  if (FAILED(hr)) {
    return LogFailedHrAndMakeError(FROM_HERE, "set title", hr);
  }

  FILEOPENDIALOGOPTIONS options;
  hr = file_open_dialog->GetOptions(&options);
  if (FAILED(hr)) {
    return LogFailedHrAndMakeError(FROM_HERE, "get options", hr);
  }
  options |= FOS_FORCEFILESYSTEM;
  hr = file_open_dialog->SetOptions(options);
  if (FAILED(hr)) {
    return LogFailedHrAndMakeError(FROM_HERE, "set options", hr);
  }

  hr = file_open_dialog->Show(nullptr);
  if (FAILED(hr)) {
    return LogFailedHrAndMakeError(FROM_HERE, "show", hr);
  }

  Microsoft::WRL::ComPtr<IShellItem> shell_item;
  hr = file_open_dialog->GetResult(&shell_item);
  if (FAILED(hr)) {
    return LogFailedHrAndMakeError(FROM_HERE, "get result", hr);
  }

  base::win::ScopedCoMem<wchar_t> path;
  hr = shell_item->GetDisplayName(SIGDN_FILESYSPATH, &path);
  if (FAILED(hr)) {
    return LogFailedHrAndMakeError(FROM_HERE, "get path", hr);
  }

  return base::FilePath(path.get());
}

}  // namespace

int FileChooserMain() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("FileChooser");

  base::win::ScopedCOMInitializer com;

  FileChooser::Result result = ShowFileChooser();

  mojo::Message serialized_message =
      mojom::FileChooserResult::SerializeAsMessage(&result);

  // Highly unlikely, but we want to know if it happens.
  if (serialized_message.data_num_bytes() > kFileChooserPipeBufferSize) {
    FileChooser::Result error_result(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
    serialized_message =
        mojom::FileChooserResult::SerializeAsMessage(&error_result);
  }

  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (stdout_handle == INVALID_HANDLE_VALUE) {
    PLOG(ERROR) << "Could not get stdout handle";
    return EXIT_FAILURE;
  }

  DWORD bytes_written;
  if (!WriteFile(stdout_handle, serialized_message.data(),
                 serialized_message.data_num_bytes(), &bytes_written,
                 nullptr)) {
    PLOG(ERROR) << "Failed to write file chooser result";
  }

  // While the pipe buffer is expected to be at least
  // kFileChooserPipeBufferSize, the parent process sets it to non-blocking just
  // in case. Check that all bytes were written successfully, and return an
  // error code if not to signal the parent that it shouldn't try to parse the
  // output.
  if (bytes_written != serialized_message.data_num_bytes()) {
    LOG(ERROR) << "Failed to write all bytes to pipe. (Buffer full?) Expected: "
               << serialized_message.data_num_bytes()
               << " Actual: " << bytes_written;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

}  // namespace remoting
