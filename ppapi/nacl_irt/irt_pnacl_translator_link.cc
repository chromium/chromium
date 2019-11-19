// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "ppapi/nacl_irt/irt_interfaces.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/proxy/ppapi_messages.h"

#if !defined(OS_NACL_NONSFI)

namespace {

typedef int (*CallbackFunc)(int nexe_fd,
                            const int* obj_file_fds,
                            int obj_file_fd_count);

class TranslatorLinkListener : public IPC::Listener {
 public:
  TranslatorLinkListener(const IPC::ChannelHandle& handle, CallbackFunc func)
      : func_(func) {
    channel_ = IPC::Channel::Create(handle, IPC::Channel::MODE_SERVER, this);
    CHECK(channel_->Connect());
  }

  // Needed for handling sync messages in OnMessageReceived().
  bool Send(IPC::Message* message) {
    return channel_->Send(message);
  }

  virtual bool OnMessageReceived(const IPC::Message& msg) {
    bool handled = false;
    IPC_BEGIN_MESSAGE_MAP(TranslatorLinkListener, msg)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiMsg_PnaclTranslatorLink,
                                      OnPnaclTranslatorLink)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 private:
  void OnPnaclTranslatorLink(
      const std::vector<ppapi::proxy::SerializedHandle>& obj_files,
      ppapi::proxy::SerializedHandle nexe_file,
      IPC::Message* reply_msg) {
    CHECK(nexe_file.is_file());

    std::vector<int> obj_file_fds(obj_files.size());
    for (size_t i = 0; i < obj_files.size(); ++i) {
      CHECK(obj_files[i].is_file());
      obj_file_fds[i] = obj_files[i].descriptor().fd;
    }
    int result = func_(nexe_file.descriptor().fd,
                       obj_file_fds.data(),
                       obj_file_fds.size());
    bool success = (result == 0);
    PpapiMsg_PnaclTranslatorLink::WriteReplyParams(reply_msg, success);
    Send(reply_msg);
  }

  std::unique_ptr<IPC::Channel> channel_;
  CallbackFunc func_;

  DISALLOW_COPY_AND_ASSIGN(TranslatorLinkListener);
};

void ServeLinkRequest(CallbackFunc func) {
  base::SingleThreadTaskExecutor main_task_executor;
  new TranslatorLinkListener(ppapi::GetRendererIPCChannelHandle(), func);
  base::RunLoop().Run();
}

}

const struct nacl_irt_private_pnacl_translator_link
    nacl_irt_private_pnacl_translator_link = {
  ServeLinkRequest
};

#endif  // !defined(OS_NACL_NONSFI)
