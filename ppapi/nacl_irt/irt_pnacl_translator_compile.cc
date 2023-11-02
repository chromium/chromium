// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "ppapi/nacl_irt/irt_interfaces.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace {

class TranslatorCompileListener : public IPC::Listener {
 public:
  TranslatorCompileListener(const IPC::ChannelHandle& handle,
                            const struct nacl_irt_pnacl_compile_funcs* funcs)
      : funcs_(funcs) {
    channel_ = IPC::Channel::Create(handle, IPC::Channel::MODE_SERVER, this);
    CHECK(channel_->Connect());
  }

  TranslatorCompileListener(const TranslatorCompileListener&) = delete;
  TranslatorCompileListener& operator=(const TranslatorCompileListener&) =
      delete;

  // Needed for handling sync messages in OnMessageReceived().
  bool Send(IPC::Message* message) {
    return channel_->Send(message);
  }

  virtual bool OnMessageReceived(const IPC::Message& msg) {
    bool handled = false;
    IPC_BEGIN_MESSAGE_MAP(TranslatorCompileListener, msg)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiMsg_PnaclTranslatorCompileInit,
                                      OnPnaclTranslatorCompileInit)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiMsg_PnaclTranslatorCompileChunk,
                                      OnPnaclTranslatorCompileChunk)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiMsg_PnaclTranslatorCompileEnd,
                                      OnPnaclTranslatorCompileEnd)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 private:
  void OnPnaclTranslatorCompileInit(
      int num_threads,
      const std::vector<ppapi::proxy::SerializedHandle>& obj_files,
      const std::vector<std::string>& cmd_flags,
      IPC::Message* reply_msg) {
    std::vector<int> obj_file_fds(obj_files.size());
    for (size_t i = 0; i < obj_files.size(); ++i) {
      CHECK(obj_files[i].is_file());
      obj_file_fds[i] = obj_files[i].descriptor().fd;
    }

    std::vector<char*> cmd_flags_cstrings(cmd_flags.size());
    for (size_t i = 0; i < cmd_flags.size(); ++i) {
      // It's OK to use const_cast here because the callee (the translator)
      // is not supposed to modify the strings.  (The interface definition
      // should have used "const char* const*".)
      cmd_flags_cstrings[i] = const_cast<char*>(cmd_flags[i].c_str());
    }

    char* error_cstr = funcs_->init_callback(num_threads,
                                             obj_file_fds.data(),
                                             obj_file_fds.size(),
                                             cmd_flags_cstrings.data(),
                                             cmd_flags_cstrings.size());
    bool success = !error_cstr;
    std::string error_str(error_cstr ? error_cstr : "");
    PpapiMsg_PnaclTranslatorCompileInit::WriteReplyParams(
        reply_msg, success, error_str);
    Send(reply_msg);
  }

  void OnPnaclTranslatorCompileChunk(const std::string& data_chunk,
                                     IPC::Message* reply_msg) {
    int result = funcs_->data_callback(data_chunk.data(), data_chunk.size());
    bool success = !result;
    PpapiMsg_PnaclTranslatorCompileChunk::WriteReplyParams(reply_msg, success);
    Send(reply_msg);
  }

  void OnPnaclTranslatorCompileEnd(IPC::Message* reply_msg) {
    char* error_cstr = funcs_->end_callback();
    bool success = !error_cstr;
    std::string error_str(error_cstr ? error_cstr : "");
    PpapiMsg_PnaclTranslatorCompileEnd::WriteReplyParams(
        reply_msg, success, error_str);
    Send(reply_msg);
  }

  std::unique_ptr<IPC::Channel> channel_;
  const struct nacl_irt_pnacl_compile_funcs* funcs_;
};

void ServeTranslateRequest(const struct nacl_irt_pnacl_compile_funcs* funcs) {
  base::SingleThreadTaskExecutor main_task_executor;
  new TranslatorCompileListener(ppapi::GetRendererIPCChannelHandle(), funcs);
  base::RunLoop().Run();
}

}

const struct nacl_irt_private_pnacl_translator_compile
    nacl_irt_private_pnacl_translator_compile = {ServeTranslateRequest};
