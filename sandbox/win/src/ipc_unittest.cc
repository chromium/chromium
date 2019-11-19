// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/sharedmem_ipc_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Helper function to make the fake shared memory with some
// basic elements initialized.
IPCControl* MakeChannels(size_t channel_size,
                         size_t total_shared_size,
                         size_t* base_start) {
  // Allocate memory
  char* mem = new char[total_shared_size];
  memset(mem, 0, total_shared_size);
  // Calculate how many channels we can fit in the shared memory.
  total_shared_size -= offsetof(IPCControl, channels);
  size_t channel_count =
      total_shared_size / (sizeof(ChannelControl) + channel_size);
  // Calculate the start of the first channel.
  *base_start =
      (sizeof(ChannelControl) * channel_count) + offsetof(IPCControl, channels);
  // Setup client structure.
  IPCControl* client_control = reinterpret_cast<IPCControl*>(mem);
  client_control->channels_count = channel_count;
  return client_control;
}

enum TestFixMode { FIX_NO_EVENTS, FIX_PONG_READY, FIX_PONG_NOT_READY };

void FixChannels(IPCControl* client_control,
                 size_t base_start,
                 size_t channel_size,
                 TestFixMode mode) {
  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    channel.channel_base = base_start;
    channel.state = kFreeChannel;
    if (mode != FIX_NO_EVENTS) {
      bool signaled = (FIX_PONG_READY == mode) ? true : false;
      channel.ping_event = ::CreateEventW(nullptr, false, false, nullptr);
      channel.pong_event = ::CreateEventW(nullptr, false, signaled, nullptr);
    }
    base_start += channel_size;
  }
}

void CloseChannelEvents(IPCControl* client_control) {
  for (size_t ix = 0; ix != client_control->channels_count; ++ix) {
    ChannelControl& channel = client_control->channels[ix];
    ::CloseHandle(channel.ping_event);
    ::CloseHandle(channel.pong_event);
  }
}

TEST(IPCTest, ChannelMaker) {
  // Test that our testing rig is computing offsets properly. We should have
  // 5 channnels and the offset to the first channel is 108 bytes in 32 bits
  // and 216 in 64 bits.
  size_t channel_start = 0;
  IPCControl* client_control = MakeChannels(12 * 64, 4096, &channel_start);
  ASSERT_TRUE(client_control);
  EXPECT_EQ(5u, client_control->channels_count);
#if defined(_WIN64)
  EXPECT_EQ(216u, channel_start);
#else
  EXPECT_EQ(108u, channel_start);
#endif
  delete[] reinterpret_cast<char*>(client_control);
}

TEST(IPCTest, ClientLockUnlock) {
  // Make 7 channels of kIPCChannelSize (1kb) each. Test that we lock and
  // unlock channels properly.
  size_t base_start = 0;
  IPCControl* client_control =
      MakeChannels(kIPCChannelSize, 4096 * 2, &base_start);
  FixChannels(client_control, base_start, kIPCChannelSize, FIX_NO_EVENTS);

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  // Test that we lock the first 3 channels in sequence.
  void* buff0 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[0].channel_base == buff0);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  void* buff1 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[1].channel_base == buff1);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  void* buff2 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[2].channel_base == buff2);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  // Test that we unlock and re-lock the right channel.
  client.FreeBuffer(buff1);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  void* buff2b = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[1].channel_base == buff2b);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  client.FreeBuffer(buff0);
  EXPECT_EQ(kFreeChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[2].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[3].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[4].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[5].state);

  delete[] reinterpret_cast<char*>(client_control);
}

TEST(IPCTest, CrossCallStrPacking) {
  // This test tries the CrossCall object with null and non-null string
  // combination of parameters, integer types and verifies that the unpacker
  // can read them properly.
  size_t base_start = 0;
  IPCControl* client_control =
      MakeChannels(kIPCChannelSize, 4096 * 4, &base_start);
  client_control->server_alive = HANDLE(1);
  FixChannels(client_control, base_start, kIPCChannelSize, FIX_PONG_READY);

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  CrossCallReturn answer;
  IpcTag tag1 = IpcTag::PING1;
  const wchar_t* text = L"98765 - 43210";
  std::wstring copied_text;
  CrossCallParamsEx* actual_params;

  CrossCall(client, tag1, text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(1u, actual_params->GetParamsCount());
  EXPECT_EQ(tag1, actual_params->GetTag());
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text));
  EXPECT_STREQ(text, copied_text.c_str());
  copied_text.clear();

  // Check with an empty string.
  IpcTag tag2 = IpcTag::PING2;
  const wchar_t* null_text = nullptr;
  CrossCall(client, tag2, null_text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(1u, actual_params->GetParamsCount());
  EXPECT_EQ(tag2, actual_params->GetTag());
  uint32_t param_size = 1;
  ArgType type = INVALID_TYPE;
  void* param_addr = actual_params->GetRawParameter(0, &param_size, &type);
  EXPECT_TRUE(param_addr);
  EXPECT_EQ(0u, param_size);
  EXPECT_EQ(WCHAR_TYPE, type);
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text));
  EXPECT_TRUE(copied_text.empty());

  IpcTag tag3 = IpcTag::PING1;
  param_size = 1;
  copied_text.clear();

  // Check with an empty string and a non-empty string.
  CrossCall(client, tag3, null_text, text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(2u, actual_params->GetParamsCount());
  EXPECT_EQ(tag3, actual_params->GetTag());
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(0, &param_size, &type);
  EXPECT_TRUE(param_addr);
  EXPECT_EQ(0u, param_size);
  EXPECT_EQ(WCHAR_TYPE, type);
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text));
  EXPECT_TRUE(copied_text.empty());
  EXPECT_TRUE(actual_params->GetParameterStr(1, &copied_text));
  EXPECT_STREQ(text, copied_text.c_str());

  param_size = 1;
  std::wstring copied_text_p0, copied_text_p2;

  const wchar_t* text2 = L"AeFG";
  CrossCall(client, tag1, text2, null_text, text, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(3u, actual_params->GetParamsCount());
  EXPECT_EQ(tag1, actual_params->GetTag());
  EXPECT_TRUE(actual_params->GetParameterStr(0, &copied_text_p0));
  EXPECT_STREQ(text2, copied_text_p0.c_str());
  EXPECT_TRUE(actual_params->GetParameterStr(2, &copied_text_p2));
  EXPECT_STREQ(text, copied_text_p2.c_str());
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(1, &param_size, &type);
  EXPECT_TRUE(param_addr);
  EXPECT_EQ(0u, param_size);
  EXPECT_EQ(WCHAR_TYPE, type);

  CloseChannelEvents(client_control);
  delete[] reinterpret_cast<char*>(client_control);
}

TEST(IPCTest, CrossCallIntPacking) {
  // Check handling for regular 32 bit integers used in Windows.
  size_t base_start = 0;
  IPCControl* client_control =
      MakeChannels(kIPCChannelSize, 4096 * 4, &base_start);
  client_control->server_alive = HANDLE(1);
  FixChannels(client_control, base_start, kIPCChannelSize, FIX_PONG_READY);

  IpcTag tag1 = IpcTag::PING1;
  IpcTag tag2 = IpcTag::PING2;
  const wchar_t* text = L"godzilla";
  CrossCallParamsEx* actual_params;

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  CrossCallReturn answer;
  DWORD dw = 0xE6578;
  CrossCall(client, tag2, dw, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(1u, actual_params->GetParamsCount());
  EXPECT_EQ(tag2, actual_params->GetTag());
  ArgType type = INVALID_TYPE;
  uint32_t param_size = 1;
  void* param_addr = actual_params->GetRawParameter(0, &param_size, &type);
  ASSERT_EQ(sizeof(dw), param_size);
  EXPECT_EQ(UINT32_TYPE, type);
  ASSERT_TRUE(param_addr);
  EXPECT_EQ(0, memcmp(&dw, param_addr, param_size));

  // Check handling for windows HANDLES.
  HANDLE h = HANDLE(0x70000500);
  CrossCall(client, tag1, text, h, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(2u, actual_params->GetParamsCount());
  EXPECT_EQ(tag1, actual_params->GetTag());
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(1, &param_size, &type);
  ASSERT_EQ(sizeof(h), param_size);
  EXPECT_EQ(VOIDPTR_TYPE, type);
  ASSERT_TRUE(param_addr);
  EXPECT_EQ(0, memcmp(&h, param_addr, param_size));

  // Check combination of 32 and 64 bits.
  CrossCall(client, tag2, h, dw, h, &answer);
  actual_params = reinterpret_cast<CrossCallParamsEx*>(client.GetBuffer());
  EXPECT_EQ(3u, actual_params->GetParamsCount());
  EXPECT_EQ(tag2, actual_params->GetTag());
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(0, &param_size, &type);
  ASSERT_EQ(sizeof(h), param_size);
  EXPECT_EQ(VOIDPTR_TYPE, type);
  ASSERT_TRUE(param_addr);
  EXPECT_EQ(0, memcmp(&h, param_addr, param_size));
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(1, &param_size, &type);
  ASSERT_EQ(sizeof(dw), param_size);
  EXPECT_EQ(UINT32_TYPE, type);
  ASSERT_TRUE(param_addr);
  EXPECT_EQ(0, memcmp(&dw, param_addr, param_size));
  type = INVALID_TYPE;
  param_addr = actual_params->GetRawParameter(2, &param_size, &type);
  ASSERT_EQ(sizeof(h), param_size);
  EXPECT_EQ(VOIDPTR_TYPE, type);
  ASSERT_TRUE(param_addr);
  EXPECT_EQ(0, memcmp(&h, param_addr, param_size));

  CloseChannelEvents(client_control);
  delete[] reinterpret_cast<char*>(client_control);
}

TEST(IPCTest, CrossCallValidation) {
  // First a sanity test with a well formed parameter object.
  unsigned long value = 124816;
  IpcTag kTag = IpcTag::PING1;
  const uint32_t kBufferSize = 256;
  ActualCallParams<1, kBufferSize> params_1(kTag);
  params_1.CopyParamIn(0, &value, sizeof(value), false, UINT32_TYPE);
  void* buffer = const_cast<void*>(params_1.GetBuffer());

  uint32_t out_size = 0;
  CrossCallParamsEx* ccp = 0;
  ccp = CrossCallParamsEx::CreateFromBuffer(buffer, params_1.GetSize(),
                                            &out_size);
  ASSERT_TRUE(ccp);
  EXPECT_TRUE(ccp->GetBuffer() != buffer);
  EXPECT_EQ(kTag, ccp->GetTag());
  EXPECT_EQ(1u, ccp->GetParamsCount());
  delete[](reinterpret_cast<char*>(ccp));

  // Test that we handle integer overflow on the number of params
  // correctly. We use a test-only ctor for ActualCallParams that
  // allows to create malformed cross-call buffers.
  const int32_t kPtrDiffSz = sizeof(ptrdiff_t);
  for (int32_t ix = -1; ix != 3; ++ix) {
    uint32_t fake_num_params = (UINT32_MAX / kPtrDiffSz) + ix;
    ActualCallParams<1, kBufferSize> params_2(kTag, fake_num_params);
    params_2.CopyParamIn(0, &value, sizeof(value), false, UINT32_TYPE);
    buffer = const_cast<void*>(params_2.GetBuffer());

    EXPECT_TRUE(buffer);
    ccp = CrossCallParamsEx::CreateFromBuffer(buffer, params_1.GetSize(),
                                              &out_size);
    // If the buffer is malformed the return is nullptr.
    EXPECT_TRUE(!ccp);
  }

  ActualCallParams<1, kBufferSize> params_3(kTag, 1);
  params_3.CopyParamIn(0, &value, sizeof(value), false, UINT32_TYPE);
  buffer = const_cast<void*>(params_3.GetBuffer());
  EXPECT_TRUE(buffer);

  uint32_t correct_size = params_3.OverrideSize(1);
  ccp = CrossCallParamsEx::CreateFromBuffer(buffer, kBufferSize, &out_size);
  EXPECT_TRUE(!ccp);

  // The correct_size is 8 bytes aligned.
  params_3.OverrideSize(correct_size - 7);
  ccp = CrossCallParamsEx::CreateFromBuffer(buffer, kBufferSize, &out_size);
  EXPECT_TRUE(!ccp);

  params_3.OverrideSize(correct_size);
  ccp = CrossCallParamsEx::CreateFromBuffer(buffer, kBufferSize, &out_size);
  EXPECT_TRUE(ccp);

  // Make sure that two parameters work as expected.
  ActualCallParams<2, kBufferSize> params_4(kTag, 2);
  params_4.CopyParamIn(0, &value, sizeof(value), false, UINT32_TYPE);
  params_4.CopyParamIn(1, buffer, sizeof(buffer), false, VOIDPTR_TYPE);
  buffer = const_cast<void*>(params_4.GetBuffer());
  EXPECT_TRUE(buffer);

  ccp = CrossCallParamsEx::CreateFromBuffer(buffer, kBufferSize, &out_size);
  EXPECT_TRUE(ccp);

#if defined(_WIN64)
  correct_size = params_4.OverrideSize(1);
  params_4.OverrideSize(correct_size - 1);
  ccp = CrossCallParamsEx::CreateFromBuffer(buffer, kBufferSize, &out_size);
  EXPECT_TRUE(!ccp);
#endif
}

// This structure is passed to the mock server threads to simulate
// the server side IPC so it has the required kernel objects.
struct ServerEvents {
  HANDLE ping;
  HANDLE pong;
  volatile LONG* state;
  HANDLE mutex;
};

// This is the server thread that quicky answers an IPC and exits.
DWORD WINAPI QuickResponseServer(PVOID param) {
  ServerEvents* events = reinterpret_cast<ServerEvents*>(param);
  DWORD wait_result = 0;
  wait_result = ::WaitForSingleObject(events->ping, INFINITE);
  ::InterlockedExchange(events->state, kAckChannel);
  ::SetEvent(events->pong);
  return wait_result;
}

class CrossCallParamsMock : public CrossCallParams {
 public:
  CrossCallParamsMock(IpcTag tag, uint32_t params_count)
      : CrossCallParams(tag, params_count) {}
};

void FakeOkAnswerInChannel(void* channel) {
  CrossCallReturn* answer = reinterpret_cast<CrossCallReturn*>(channel);
  answer->call_outcome = SBOX_ALL_OK;
}

// Create two threads that will quickly answer IPCs; the first one
// using channel 1 (channel 0 is busy) and one using channel 0. No time-out
// should occur.
TEST(IPCTest, ClientFastServer) {
  const size_t channel_size = kIPCChannelSize;
  size_t base_start = 0;
  IPCControl* client_control =
      MakeChannels(channel_size, 4096 * 2, &base_start);
  FixChannels(client_control, base_start, kIPCChannelSize, FIX_PONG_NOT_READY);
  client_control->server_alive = ::CreateMutex(nullptr, false, nullptr);

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  ServerEvents events = {0};
  events.ping = client_control->channels[1].ping_event;
  events.pong = client_control->channels[1].pong_event;
  events.state = &client_control->channels[1].state;

  HANDLE t1 =
      ::CreateThread(nullptr, 0, QuickResponseServer, &events, 0, nullptr);
  ASSERT_TRUE(t1);
  ::CloseHandle(t1);

  void* buff0 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[0].channel_base == buff0);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  void* buff1 = client.GetBuffer();
  EXPECT_TRUE(mem + client_control->channels[1].channel_base == buff1);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kBusyChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  EXPECT_EQ(IpcTag::UNUSED, client_control->channels[1].ipc_tag);

  IpcTag tag = IpcTag::PING1;
  CrossCallReturn answer;
  CrossCallParamsMock* params1 = new (buff1) CrossCallParamsMock(tag, 1);
  FakeOkAnswerInChannel(buff1);

  ResultCode result = client.DoCall(params1, &answer);
  if (SBOX_ERROR_CHANNEL_ERROR != result)
    client.FreeBuffer(buff1);

  EXPECT_TRUE(SBOX_ALL_OK == result);
  EXPECT_EQ(tag, client_control->channels[1].ipc_tag);
  EXPECT_EQ(kBusyChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  HANDLE t2 =
      ::CreateThread(nullptr, 0, QuickResponseServer, &events, 0, nullptr);
  ASSERT_TRUE(t2);
  ::CloseHandle(t2);

  client.FreeBuffer(buff0);
  events.ping = client_control->channels[0].ping_event;
  events.pong = client_control->channels[0].pong_event;
  events.state = &client_control->channels[0].state;

  tag = IpcTag::PING2;
  CrossCallParamsMock* params2 = new (buff0) CrossCallParamsMock(tag, 1);
  FakeOkAnswerInChannel(buff0);

  result = client.DoCall(params2, &answer);
  if (SBOX_ERROR_CHANNEL_ERROR != result)
    client.FreeBuffer(buff0);

  EXPECT_TRUE(SBOX_ALL_OK == result);
  EXPECT_EQ(tag, client_control->channels[0].ipc_tag);
  EXPECT_EQ(kFreeChannel, client_control->channels[0].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[1].state);
  EXPECT_EQ(kFreeChannel, client_control->channels[2].state);

  CloseChannelEvents(client_control);
  ::CloseHandle(client_control->server_alive);

  delete[] reinterpret_cast<char*>(client_control);
}

// This is the server thread that very slowly answers an IPC and exits. Note
// that the pong event needs to be signaled twice.
DWORD WINAPI SlowResponseServer(PVOID param) {
  ServerEvents* events = reinterpret_cast<ServerEvents*>(param);
  DWORD wait_result = 0;
  wait_result = ::WaitForSingleObject(events->ping, INFINITE);
  ::Sleep(kIPCWaitTimeOut1 + kIPCWaitTimeOut2 + 200);
  ::InterlockedExchange(events->state, kAckChannel);
  ::SetEvent(events->pong);
  return wait_result;
}

// This thread's job is to keep the mutex locked.
DWORD WINAPI MainServerThread(PVOID param) {
  ServerEvents* events = reinterpret_cast<ServerEvents*>(param);
  DWORD wait_result = 0;
  wait_result = ::WaitForSingleObject(events->mutex, INFINITE);
  Sleep(kIPCWaitTimeOut1 * 20);
  return wait_result;
}

// Creates a server thread that answers the IPC so slow that is guaranteed to
// trigger the time-out code path in the client. A second thread is created
// to hold locked the server_alive mutex: this signals the client that the
// server is not dead and it retries the wait.
TEST(IPCTest, ClientSlowServer) {
  size_t base_start = 0;
  IPCControl* client_control =
      MakeChannels(kIPCChannelSize, 4096 * 2, &base_start);
  FixChannels(client_control, base_start, kIPCChannelSize, FIX_PONG_NOT_READY);
  client_control->server_alive = ::CreateMutex(nullptr, false, nullptr);

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  ServerEvents events = {0};
  events.ping = client_control->channels[0].ping_event;
  events.pong = client_control->channels[0].pong_event;
  events.state = &client_control->channels[0].state;

  HANDLE t1 =
      ::CreateThread(nullptr, 0, SlowResponseServer, &events, 0, nullptr);
  ASSERT_TRUE(t1);
  ::CloseHandle(t1);

  ServerEvents events2 = {0};
  events2.pong = events.pong;
  events2.mutex = client_control->server_alive;

  HANDLE t2 =
      ::CreateThread(nullptr, 0, MainServerThread, &events2, 0, nullptr);
  ASSERT_TRUE(t2);
  ::CloseHandle(t2);

  ::Sleep(1);

  void* buff0 = client.GetBuffer();
  IpcTag tag = IpcTag::PING1;
  CrossCallReturn answer;
  CrossCallParamsMock* params1 = new (buff0) CrossCallParamsMock(tag, 1);
  FakeOkAnswerInChannel(buff0);

  ResultCode result = client.DoCall(params1, &answer);
  if (SBOX_ERROR_CHANNEL_ERROR != result)
    client.FreeBuffer(buff0);

  EXPECT_TRUE(SBOX_ALL_OK == result);
  EXPECT_EQ(tag, client_control->channels[0].ipc_tag);
  EXPECT_EQ(kFreeChannel, client_control->channels[0].state);

  CloseChannelEvents(client_control);
  ::CloseHandle(client_control->server_alive);
  delete[] reinterpret_cast<char*>(client_control);
}

// This test-only IPC dispatcher has two handlers with the same signature
// but only CallOneHandler should be used.
class UnitTestIPCDispatcher : public Dispatcher {
 public:
  UnitTestIPCDispatcher();
  ~UnitTestIPCDispatcher() override {}

  bool SetupService(InterceptionManager* manager, IpcTag service) override {
    return true;
  }

 private:
  bool CallOneHandler(IPCInfo* ipc, HANDLE p1, uint32_t p2) {
    ipc->return_info.extended[0].handle = p1;
    ipc->return_info.extended[1].unsigned_int = p2;
    return true;
  }

  bool CallTwoHandler(IPCInfo* ipc, HANDLE p1, uint32_t p2) { return true; }
};

UnitTestIPCDispatcher::UnitTestIPCDispatcher() {
  static const IPCCall call_one = {{IpcTag::PING1, {VOIDPTR_TYPE, UINT32_TYPE}},
                                   reinterpret_cast<CallbackGeneric>(
                                       &UnitTestIPCDispatcher::CallOneHandler)};
  static const IPCCall call_two = {{IpcTag::PING2, {VOIDPTR_TYPE, UINT32_TYPE}},
                                   reinterpret_cast<CallbackGeneric>(
                                       &UnitTestIPCDispatcher::CallTwoHandler)};
  ipc_calls_.push_back(call_one);
  ipc_calls_.push_back(call_two);
}

// This test does most of the shared memory IPC client-server roundtrip
// and tests the packing, unpacking and call dispatching.
TEST(IPCTest, SharedMemServerTests) {
  size_t base_start = 0;
  IPCControl* client_control = MakeChannels(kIPCChannelSize, 4096, &base_start);
  client_control->server_alive = HANDLE(1);
  FixChannels(client_control, base_start, kIPCChannelSize, FIX_PONG_READY);

  char* mem = reinterpret_cast<char*>(client_control);
  SharedMemIPCClient client(mem);

  CrossCallReturn answer;
  HANDLE bar = HANDLE(191919);
  DWORD foo = 6767676;
  CrossCall(client, IpcTag::PING1, bar, foo, &answer);
  void* buff = client.GetBuffer();
  ASSERT_TRUE(buff);

  UnitTestIPCDispatcher dispatcher;
  // Since we are directly calling InvokeCallback, most of this structure
  // can be set to nullptr.
  sandbox::SharedMemIPCServer::ServerControl srv_control = {};
  srv_control.channel_size = kIPCChannelSize;
  srv_control.shared_base = reinterpret_cast<char*>(client_control);
  srv_control.dispatcher = &dispatcher;

  sandbox::CrossCallReturn call_return = {0};
  EXPECT_TRUE(
      SharedMemIPCServer::InvokeCallback(&srv_control, buff, &call_return));
  EXPECT_EQ(SBOX_ALL_OK, call_return.call_outcome);
  EXPECT_TRUE(bar == call_return.extended[0].handle);
  EXPECT_EQ(foo, call_return.extended[1].unsigned_int);

  CloseChannelEvents(client_control);
  delete[] reinterpret_cast<char*>(client_control);
}

}  // namespace sandbox
