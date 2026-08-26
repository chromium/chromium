// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <cstdlib>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/file_transfer/file_chooser.h"
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

class FileChooserImpl : public mojom::FileChooser {
 public:
  FileChooserImpl(mojo::PendingReceiver<mojom::FileChooser> receiver,
                  base::OnceClosure on_done)
      : receiver_(this, std::move(receiver)), on_done_(std::move(on_done)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&FileChooserImpl::OnDisconnect, base::Unretained(this)));
  }

  FileChooserImpl(const FileChooserImpl&) = delete;
  FileChooserImpl& operator=(const FileChooserImpl&) = delete;

  ~FileChooserImpl() override = default;

  // mojom::FileChooser implementation.
  void OpenFile(OpenFileCallback callback) override {
    base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
        ->PostTaskAndReplyWithResult(
            FROM_HERE, base::BindOnce(&ShowFileChooser), std::move(callback));
  }

 private:
  void OnDisconnect() {
    if (on_done_) {
      std::move(on_done_).Run();
    }
  }

  mojo::Receiver<mojom::FileChooser> receiver_;
  base::OnceClosure on_done_;
};

}  // namespace

int FileChooserMain() {
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("FileChooser");

  base::win::ScopedCOMInitializer com;

  mojo::core::ScopedIPCSupport ipc_support(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  mojo::PlatformChannelEndpoint endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess());
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Invalid Mojo channel endpoint.";
    return EXIT_FAILURE;
  }

  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  mojo::ScopedMessagePipeHandle pipe = invitation.ExtractMessagePipe(0);
  if (!pipe.is_valid()) {
    LOG(ERROR) << "Invalid Mojo message pipe.";
    return EXIT_FAILURE;
  }

  base::RunLoop run_loop;
  FileChooserImpl file_chooser(
      mojo::PendingReceiver<mojom::FileChooser>(std::move(pipe)),
      run_loop.QuitClosure());

  run_loop.Run();
  return EXIT_SUCCESS;
}

}  // namespace remoting
