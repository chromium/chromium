// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/udp_socket_filter.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/error_conversion.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/udp_socket_resource_constants.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace proxy {

namespace {

int32_t SetRecvFromOutput(PP_Instance pp_instance,
                          const std::unique_ptr<std::string>& data,
                          const PP_NetAddress_Private& addr,
                          char* output_buffer,
                          int32_t num_bytes,
                          PP_Resource* output_addr,
                          int32_t browser_result) {
  ProxyLock::AssertAcquired();
  DCHECK_GE(num_bytes, static_cast<int32_t>(data->size()));

  int32_t result = browser_result;
  if (result == PP_OK && output_addr) {
    thunk::EnterResourceCreationNoLock enter(pp_instance);
    if (enter.succeeded()) {
      *output_addr = enter.functions()->CreateNetAddressFromNetAddressPrivate(
          pp_instance, addr);
    } else {
      result = PP_ERROR_FAILED;
    }
  }

  if (result == PP_OK && !data->empty())
    memcpy(output_buffer, data->c_str(), data->size());

  return result == PP_OK ? static_cast<int32_t>(data->size()) : result;
}

}  // namespace

UDPSocketFilter::UDPSocketFilter() {
}

UDPSocketFilter::~UDPSocketFilter() {
}

void UDPSocketFilter::AddUDPResource(
    PP_Instance instance,
    PP_Resource resource,
    bool private_api,
    base::RepeatingClosure slot_available_callback) {
  ProxyLock::AssertAcquired();
  base::AutoLock acquire(lock_);
  DCHECK(queues_.find(resource) == queues_.end());
  queues_[resource] = std::make_unique<RecvQueue>(
      instance, private_api, std::move(slot_available_callback));
}

void UDPSocketFilter::RemoveUDPResource(PP_Resource resource) {
  ProxyLock::AssertAcquired();
  base::AutoLock acquire(lock_);
  auto erase_count = queues_.erase(resource);
  DCHECK_GT(erase_count, 0u);
}

int32_t UDPSocketFilter::RequestData(
    PP_Resource resource,
    int32_t num_bytes,
    char* buffer,
    PP_Resource* addr,
    const scoped_refptr<TrackedCallback>& callback) {
  ProxyLock::AssertAcquired();
  base::AutoLock acquire(lock_);
  auto it = queues_.find(resource);
  if (it == queues_.end()) {
    NOTREACHED();
  }
  return it->second->RequestData(num_bytes, buffer, addr, callback);
}

bool UDPSocketFilter::OnResourceReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& nested_msg) {
  bool handled = true;
  PPAPI_BEGIN_MESSAGE_MAP(UDPSocketFilter, nested_msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(PpapiPluginMsg_UDPSocket_PushRecvResult,
                                        OnPluginMsgPushRecvResult)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(handled = false)
  PPAPI_END_MESSAGE_MAP()
  return handled;
}

PP_NetAddress_Private UDPSocketFilter::GetLastAddrPrivate(
    PP_Resource resource) const {
  base::AutoLock acquire(lock_);
  auto it = queues_.find(resource);
  return it->second->GetLastAddrPrivate();
}

void UDPSocketFilter::OnPluginMsgPushRecvResult(
    const ResourceMessageReplyParams& params,
    int32_t result,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK(PluginGlobals::Get()->ipc_task_runner()->RunsTasksInCurrentSequence());
  base::AutoLock acquire(lock_);
  auto it = queues_.find(params.pp_resource());
  // The RecvQueue might be gone if there were messages in-flight for a
  // resource that has been destroyed.
  if (it != queues_.end()) {
    // TODO(yzshen): Support passing in a non-const string ref, so that we can
    // eliminate one copy when storing the data in the buffer.
    it->second->DataReceivedOnIOThread(result, data, addr);
  }
}

UDPSocketFilter::RecvQueue::RecvQueue(
    PP_Instance pp_instance,
    bool private_api,
    base::RepeatingClosure slot_available_callback)
    : pp_instance_(pp_instance),
      read_buffer_(nullptr),
      bytes_to_read_(0),
      recvfrom_addr_resource_(nullptr),
      last_recvfrom_addr_(),
      private_api_(private_api),
      slot_available_callback_(std::move(slot_available_callback)) {}

UDPSocketFilter::RecvQueue::~RecvQueue() {
  if (TrackedCallback::IsPending(recvfrom_callback_))
    recvfrom_callback_->PostAbort();
}

void UDPSocketFilter::RecvQueue::DataReceivedOnIOThread(
    int32_t result,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK(PluginGlobals::Get()->ipc_task_runner()->RunsTasksInCurrentSequence());
  DCHECK_LT(recv_buffers_.size(),
            static_cast<size_t>(
                UDPSocketResourceConstants::kPluginReceiveBufferSlots));

  if (!TrackedCallback::IsPending(recvfrom_callback_) || !read_buffer_) {
    recv_buffers_.push(RecvBuffer());
    RecvBuffer& back = recv_buffers_.back();
    back.result = result;
    back.data = data;
    back.addr = addr;
    return;
  }
  DCHECK_EQ(recv_buffers_.size(), 0u);

  if (bytes_to_read_ < static_cast<int32_t>(data.size())) {
    recv_buffers_.push(RecvBuffer());
    RecvBuffer& back = recv_buffers_.back();
    back.result = result;
    back.data = data;
    back.addr = addr;

    result = PP_ERROR_MESSAGE_TOO_BIG;
  } else {
    // Instead of calling SetRecvFromOutput directly, post it as a completion
    // task, so that:
    // 1) It can run with the ProxyLock (we can't lock it on the IO thread.)
    // 2) So that we only write to the output params in the case of success.
    //    (Since the callback will complete on another thread, it's possible
    //     that the resource will be deleted and abort the callback before it
    //     is actually run.)
    std::unique_ptr<std::string> data_to_pass(new std::string(data));
    recvfrom_callback_->set_completion_task(base::BindOnce(
        &SetRecvFromOutput, pp_instance_, std::move(data_to_pass), addr,
        base::Unretained(read_buffer_), bytes_to_read_,
        base::Unretained(recvfrom_addr_resource_)));
    last_recvfrom_addr_ = addr;
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, RunWhileLocked(base::BindOnce(slot_available_callback_)));
  }

  read_buffer_ = NULL;
  bytes_to_read_ = -1;
  recvfrom_addr_resource_ = NULL;

  recvfrom_callback_->Run(
      ConvertNetworkAPIErrorForCompatibility(result, private_api_));
}

int32_t UDPSocketFilter::RecvQueue::RequestData(
    int32_t num_bytes,
    char* buffer_out,
    PP_Resource* addr_out,
    const scoped_refptr<TrackedCallback>& callback) {
  ProxyLock::AssertAcquired();
  if (!buffer_out || num_bytes <= 0)
    return PP_ERROR_BADARGUMENT;
  if (TrackedCallback::IsPending(recvfrom_callback_))
    return PP_ERROR_INPROGRESS;

  if (recv_buffers_.empty()) {
    read_buffer_ = buffer_out;
    bytes_to_read_ = std::min(
        num_bytes,
        static_cast<int32_t>(UDPSocketResourceConstants::kMaxReadSize));
    recvfrom_addr_resource_ = addr_out;
    recvfrom_callback_ = callback;
    return PP_OK_COMPLETIONPENDING;
  } else {
    RecvBuffer& front = recv_buffers_.front();

    if (static_cast<size_t>(num_bytes) < front.data.size())
      return PP_ERROR_MESSAGE_TOO_BIG;

    std::unique_ptr<std::string> data_to_pass(new std::string);
    data_to_pass->swap(front.data);
    int32_t result =
        SetRecvFromOutput(pp_instance_, std::move(data_to_pass), front.addr,
                          buffer_out, num_bytes, addr_out, front.result);
    last_recvfrom_addr_ = front.addr;
    recv_buffers_.pop();
    slot_available_callback_.Run();

    return result;
  }
}

PP_NetAddress_Private UDPSocketFilter::RecvQueue::GetLastAddrPrivate() const {
  CHECK(private_api_);
  return last_recvfrom_addr_;
}

}  // namespace proxy
}  // namespace ppapi
