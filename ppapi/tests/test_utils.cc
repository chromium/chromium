// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/private/host_resolver_private.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var.h"

namespace {

bool IsBigEndian() {
  union {
    uint32_t integer32;
    uint8_t integer8[4];
  } data = { 0x01020304 };

  return data.integer8[0] == 1;
}

void DoNothing(void* user_data, int32_t result) {}

}  // namespace

const int kActionTimeoutMs = 10000;

const PPB_Testing_Private* GetTestingInterface() {
  static const PPB_Testing_Private* g_testing_interface =
      static_cast<const PPB_Testing_Private*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_TESTING_PRIVATE_INTERFACE));
  return g_testing_interface;
}

std::string ReportError(const char* method, int32_t error) {
  char error_as_string[12];
  sprintf(error_as_string, "%d", static_cast<int>(error));
  std::string result = method + std::string(" failed with error: ") +
      error_as_string;
  return result;
}

void PlatformSleep(int duration_ms) {
#if defined(_MSC_VER)
  ::Sleep(duration_ms);
#else
  usleep(duration_ms * 1000);
#endif
}

bool GetLocalHostPort(PP_Instance instance, std::string* host, uint16_t* port) {
  if (!host || !port)
    return false;

  const PPB_Testing_Private* testing = GetTestingInterface();
  if (!testing)
    return false;

  PP_URLComponents_Dev components;
  pp::Var pp_url(pp::PASS_REF,
                 testing->GetDocumentURL(instance, &components));
  if (!pp_url.is_string())
    return false;
  std::string url = pp_url.AsString();

  if (components.host.len < 0)
    return false;
  host->assign(url.substr(components.host.begin, components.host.len));

  if (components.port.len <= 0)
    return false;

  int i = atoi(url.substr(components.port.begin, components.port.len).c_str());
  if (i < 0 || i > 65535)
    return false;
  *port = static_cast<uint16_t>(i);

  return true;
}

uint16_t ConvertFromNetEndian16(uint16_t x) {
  if (IsBigEndian())
    return x;
  else
    return (x << 8) | (x >> 8);
}

uint16_t ConvertToNetEndian16(uint16_t x) {
  if (IsBigEndian())
    return x;
  else
    return (x << 8) | (x >> 8);
}

bool EqualNetAddress(const pp::NetAddress& addr1, const pp::NetAddress& addr2) {
  if (addr1.GetFamily() == PP_NETADDRESS_FAMILY_UNSPECIFIED ||
      addr2.GetFamily() == PP_NETADDRESS_FAMILY_UNSPECIFIED) {
    return false;
  }

  if (addr1.GetFamily() == PP_NETADDRESS_FAMILY_IPV4) {
    PP_NetAddress_IPv4 ipv4_addr1, ipv4_addr2;
    if (!addr1.DescribeAsIPv4Address(&ipv4_addr1) ||
        !addr2.DescribeAsIPv4Address(&ipv4_addr2)) {
      return false;
    }

    return ipv4_addr1.port == ipv4_addr2.port &&
           !memcmp(ipv4_addr1.addr, ipv4_addr2.addr, sizeof(ipv4_addr1.addr));
  } else {
    PP_NetAddress_IPv6 ipv6_addr1, ipv6_addr2;
    if (!addr1.DescribeAsIPv6Address(&ipv6_addr1) ||
        !addr2.DescribeAsIPv6Address(&ipv6_addr2)) {
      return false;
    }

    return ipv6_addr1.port == ipv6_addr2.port &&
           !memcmp(ipv6_addr1.addr, ipv6_addr2.addr, sizeof(ipv6_addr1.addr));
  }
}

bool ResolveHost(PP_Instance instance,
                 const std::string& host,
                 uint16_t port,
                 pp::NetAddress* addr) {
  // TODO(yzshen): Change to use the public host resolver once it is supported.
  pp::InstanceHandle instance_handle(instance);
  pp::HostResolverPrivate host_resolver(instance_handle);
  PP_HostResolver_Private_Hint hint =
      { PP_NETADDRESSFAMILY_PRIVATE_UNSPECIFIED, 0 };

  TestCompletionCallback callback(instance);
  callback.WaitForResult(
      host_resolver.Resolve(host, port, hint, callback.GetCallback()));

  PP_NetAddress_Private addr_private;
  if (callback.result() != PP_OK || host_resolver.GetSize() == 0 ||
      !host_resolver.GetNetAddress(0, &addr_private)) {
    return false;
  }

  switch (pp::NetAddressPrivate::GetFamily(addr_private)) {
    case PP_NETADDRESSFAMILY_PRIVATE_IPV4: {
      PP_NetAddress_IPv4 ipv4_addr;
      ipv4_addr.port = ConvertToNetEndian16(
          pp::NetAddressPrivate::GetPort(addr_private));
      if (!pp::NetAddressPrivate::GetAddress(addr_private, ipv4_addr.addr,
                                             sizeof(ipv4_addr.addr))) {
        return false;
      }
      *addr = pp::NetAddress(instance_handle, ipv4_addr);
      return true;
    }
    case PP_NETADDRESSFAMILY_PRIVATE_IPV6: {
      PP_NetAddress_IPv6 ipv6_addr;
      ipv6_addr.port = ConvertToNetEndian16(
          pp::NetAddressPrivate::GetPort(addr_private));
      if (!pp::NetAddressPrivate::GetAddress(addr_private, ipv6_addr.addr,
                                             sizeof(ipv6_addr.addr))) {
        return false;
      }
      *addr = pp::NetAddress(instance_handle, ipv6_addr);
      return true;
    }
    default: {
      return false;
    }
  }
}

bool ReplacePort(PP_Instance instance,
                 const pp::NetAddress& input_addr,
                 uint16_t port,
                 pp::NetAddress* output_addr) {
  switch (input_addr.GetFamily()) {
    case PP_NETADDRESS_FAMILY_IPV4: {
      PP_NetAddress_IPv4 ipv4_addr;
      if (!input_addr.DescribeAsIPv4Address(&ipv4_addr))
        return false;
      ipv4_addr.port = ConvertToNetEndian16(port);
      *output_addr = pp::NetAddress(pp::InstanceHandle(instance), ipv4_addr);
      return true;
    }
    case PP_NETADDRESS_FAMILY_IPV6: {
      PP_NetAddress_IPv6 ipv6_addr;
      if (!input_addr.DescribeAsIPv6Address(&ipv6_addr))
        return false;
      ipv6_addr.port = ConvertToNetEndian16(port);
      *output_addr = pp::NetAddress(pp::InstanceHandle(instance), ipv6_addr);
      return true;
    }
    default: {
      return false;
    }
  }
}

uint16_t GetPort(const pp::NetAddress& addr) {
  switch (addr.GetFamily()) {
    case PP_NETADDRESS_FAMILY_IPV4: {
      PP_NetAddress_IPv4 ipv4_addr;
      if (!addr.DescribeAsIPv4Address(&ipv4_addr))
        return 0;
      return ConvertFromNetEndian16(ipv4_addr.port);
    }
    case PP_NETADDRESS_FAMILY_IPV6: {
      PP_NetAddress_IPv6 ipv6_addr;
      if (!addr.DescribeAsIPv6Address(&ipv6_addr))
        return 0;
      return ConvertFromNetEndian16(ipv6_addr.port);
    }
    default: {
      return 0;
    }
  }
}

void NestedEvent::Wait() {
  PP_DCHECK(pp::Module::Get()->core()->IsMainThread());
  // Don't allow nesting more than once; it doesn't work with the code as-is,
  // and probably is a bad idea most of the time anyway.
  PP_DCHECK(!waiting_);
  if (signalled_)
    return;
  waiting_ = true;
  while (!signalled_)
    GetTestingInterface()->RunMessageLoop(instance_);
  waiting_ = false;
}

void NestedEvent::Signal() {
  if (pp::Module::Get()->core()->IsMainThread())
    SignalOnMainThread();
  else
    PostSignal(0);
}

void NestedEvent::PostSignal(int32_t wait_ms) {
  pp::Module::Get()->core()->CallOnMainThread(
      wait_ms,
      pp::CompletionCallback(&SignalThunk, this),
      0);
}

void NestedEvent::Reset() {
  PP_DCHECK(pp::Module::Get()->core()->IsMainThread());
  // It doesn't make sense to reset when we're still waiting.
  PP_DCHECK(!waiting_);
  signalled_ = false;
}

void NestedEvent::SignalOnMainThread() {
  PP_DCHECK(pp::Module::Get()->core()->IsMainThread());
  signalled_ = true;
  if (waiting_)
    GetTestingInterface()->QuitMessageLoop(instance_);
}

void NestedEvent::SignalThunk(void* event, int32_t /* result */) {
  static_cast<NestedEvent*>(event)->SignalOnMainThread();
}

pp::CompletionCallback DoNothingCallback() {
  return pp::CompletionCallback(&DoNothing, NULL,
                                PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
}

TestCompletionCallback::TestCompletionCallback(PP_Instance instance)
    : wait_for_result_called_(false),
      have_result_(false),
      result_(PP_OK_COMPLETIONPENDING),
      // TODO(dmichael): The default should probably be PP_REQUIRED, but this is
      //                 what the tests currently expect.
      callback_type_(PP_OPTIONAL),
      post_quit_task_(false),
      instance_(instance),
      delegate_(NULL) {
}

TestCompletionCallback::TestCompletionCallback(PP_Instance instance,
                                               bool force_async)
    : wait_for_result_called_(false),
      have_result_(false),
      result_(PP_OK_COMPLETIONPENDING),
      callback_type_(force_async ? PP_REQUIRED : PP_OPTIONAL),
      post_quit_task_(false),
      instance_(instance),
      delegate_(NULL) {
}

TestCompletionCallback::TestCompletionCallback(PP_Instance instance,
                                               CallbackType callback_type)
    : wait_for_result_called_(false),
      have_result_(false),
      result_(PP_OK_COMPLETIONPENDING),
      callback_type_(callback_type),
      post_quit_task_(false),
      instance_(instance),
      delegate_(NULL) {
}

void TestCompletionCallback::WaitForResult(int32_t result) {
  PP_DCHECK(!wait_for_result_called_);
  wait_for_result_called_ = true;
  errors_.clear();
  if (result == PP_OK_COMPLETIONPENDING) {
    if (!have_result_) {
      post_quit_task_ = true;
      RunMessageLoop();
    }
    if (callback_type_ == PP_BLOCKING) {
      errors_.assign(
          ReportError("TestCompletionCallback: Call did not run synchronously "
                      "when passed a blocking completion callback!",
                      result_));
      return;
    }
  } else {
    result_ = result;
    have_result_ = true;
    if (callback_type_ == PP_REQUIRED) {
      errors_.assign(
          ReportError("TestCompletionCallback: Call ran synchronously when "
                      "passed a required completion callback!",
                      result_));
      return;
    }
  }
  PP_DCHECK(have_result_ == true);
}

void TestCompletionCallback::WaitForAbortResult(int32_t result) {
  WaitForResult(result);
  int32_t final_result = result_;
  if (result == PP_OK_COMPLETIONPENDING) {
    if (final_result != PP_ERROR_ABORTED) {
      errors_.assign(
          ReportError("TestCompletionCallback: Expected PP_ERROR_ABORTED or "
                      "PP_OK. Ran asynchronously.",
                      final_result));
      return;
    }
  } else if (result < PP_OK) {
    errors_.assign(
        ReportError("TestCompletionCallback: Expected PP_ERROR_ABORTED or "
                    "non-error response. Ran synchronously.",
                    result));
    return;
  }
}

pp::CompletionCallback TestCompletionCallback::GetCallback() {
  Reset();
  int32_t flags = 0;
  if (callback_type_ == PP_BLOCKING)
    return pp::CompletionCallback();
  else if (callback_type_ == PP_OPTIONAL)
    flags = PP_COMPLETIONCALLBACK_FLAG_OPTIONAL;
  target_loop_ = pp::MessageLoop::GetCurrent();
  return pp::CompletionCallback(&TestCompletionCallback::Handler,
                                const_cast<TestCompletionCallback*>(this),
                                flags);
}

void TestCompletionCallback::Reset() {
  wait_for_result_called_ = false;
  result_ = PP_OK_COMPLETIONPENDING;
  have_result_ = false;
  post_quit_task_ = false;
  delegate_ = NULL;
  errors_.clear();
}

// static
void TestCompletionCallback::Handler(void* user_data, int32_t result) {
  TestCompletionCallback* callback =
      static_cast<TestCompletionCallback*>(user_data);
  // If this check fails, it means that the callback was invoked twice or that
  // the PPAPI call completed synchronously, but also ran the callback.
  PP_DCHECK(!callback->have_result_);
  callback->result_ = result;
  callback->have_result_ = true;
  if (callback->delegate_)
    callback->delegate_->OnCallback(user_data, result);
  if (callback->post_quit_task_) {
    callback->post_quit_task_ = false;
    callback->QuitMessageLoop();
  }
  if (callback->target_loop_ != pp::MessageLoop::GetCurrent()) {
    // Note, in-process, loop_ and GetCurrent() will both be NULL, so should
    // still be equal.
    callback->errors_.assign(
        ReportError("TestCompletionCallback: Callback ran on the wrong message "
                    "loop!",
                    result));
  }
}

void TestCompletionCallback::RunMessageLoop() {
  pp::MessageLoop loop(pp::MessageLoop::GetCurrent());
  // If we don't have a message loop, we're probably running in process, where
  // PPB_MessageLoop is not supported. Just use the Testing message loop.
  if (loop.is_null() || loop == pp::MessageLoop::GetForMainThread())
    GetTestingInterface()->RunMessageLoop(instance_);
  else
    loop.Run();
}

void TestCompletionCallback::QuitMessageLoop() {
  pp::MessageLoop loop(pp::MessageLoop::GetCurrent());
  // If we don't have a message loop, we're probably running in process, where
  // PPB_MessageLoop is not supported. Just use the Testing message loop.
  if (loop.is_null() || loop == pp::MessageLoop::GetForMainThread()) {
    GetTestingInterface()->QuitMessageLoop(instance_);
  } else {
    const bool should_quit = false;
    loop.PostQuit(should_quit);
  }
}

int32_t OpenURLRequest(PP_Instance instance,
                       pp::URLLoader* loader,
                       const pp::URLRequestInfo& request,
                       CallbackType callback_type,
                       std::string* response_body) {
  {
    TestCompletionCallback open_callback(instance, callback_type);
    open_callback.WaitForResult(
        loader->Open(request, open_callback.GetCallback()));
    if (open_callback.result() != PP_OK)
      return open_callback.result();
  }

  int32_t bytes_read = 0;
  do {
    char buffer[1024];
    TestCompletionCallback read_callback(instance, callback_type);
    read_callback.WaitForResult(loader->ReadResponseBody(
        &buffer, sizeof(buffer), read_callback.GetCallback()));
    bytes_read = read_callback.result();
    if (bytes_read < 0)
      return bytes_read;
    if (response_body)
      response_body->append(std::string(buffer, bytes_read));
  } while (bytes_read > 0);

  return PP_OK;
}
