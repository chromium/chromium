// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/proxy/file_system_resource.h"
#include "ppapi/proxy/locking_resource_releaser.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::proxy::ResourceMessageTestSink;
using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_FileSystem_API;

namespace ppapi {
namespace proxy {

namespace {

const int64_t kExpectedFileSystemSize = 100;
const int64_t kQuotaRequestAmount1 = 10;
const int64_t kQuotaRequestAmount2 = 20;

class MockCompletionCallback {
 public:
  MockCompletionCallback() : called_(false) {}

  bool called() { return called_; }
  int32_t result() { return result_; }

  static void Callback(void* user_data, int32_t result) {
    MockCompletionCallback* that =
        reinterpret_cast<MockCompletionCallback*>(user_data);
    that->called_ = true;
    that->result_ = result;
  }

 private:
  bool called_;
  int32_t result_;
};

class MockRequestQuotaCallback {
 public:
  MockRequestQuotaCallback() : called_(false) {}

  bool called() { return called_; }
  int64_t result() { return result_; }

  void Reset() { called_ = false; }

  void Callback(int64_t result) {
    ASSERT_FALSE(called_);
    called_ = true;
    result_ = result;
  }

 private:
  bool called_;
  int64_t result_;
};

class FileSystemResourceTest : public PluginProxyTest {
 public:
  const PPB_FileSystem_1_0* file_system_iface;
  const PPB_FileRef_1_1* file_ref_iface;
  const PPB_FileIO_1_1* file_io_iface;

  FileSystemResourceTest()
      : file_system_iface(thunk::GetPPB_FileSystem_1_0_Thunk()),
        file_ref_iface(thunk::GetPPB_FileRef_1_1_Thunk()),
        file_io_iface(thunk::GetPPB_FileIO_1_1_Thunk()) {
  }

  void SendReply(const ResourceMessageCallParams& params,
                 int32_t result,
                 const IPC::Message& nested_message) {
    ResourceMessageReplyParams reply_params(params.pp_resource(),
                                            params.sequence());
    reply_params.set_result(result);
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params, nested_message);
  }

  void SendOpenReply(const ResourceMessageCallParams& params, int32_t result) {
    SendReply(params, result, PpapiPluginMsg_FileSystem_OpenReply());
  }

  // Opens the given file system.
  void OpenFileSystem(PP_Resource file_system) {
    MockCompletionCallback cb;
    int32_t result = file_system_iface->Open(
        file_system,
        kExpectedFileSystemSize,
        PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    // Should have sent two new "open" messages to the browser and renderer.
    ResourceMessageTestSink::ResourceCallVector open_messages =
        sink().GetAllResourceCallsMatching(PpapiHostMsg_FileSystem_Open::ID);
    ASSERT_EQ(2U, open_messages.size());
    sink().ClearMessages();

    // The resource is expecting two replies.
    SendOpenReply(open_messages[0].first, PP_OK);
    SendOpenReply(open_messages[1].first, PP_OK);

    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_OK, cb.result());
  }

  // Opens the given file in the given file system. Since there is no host,
  // the file handle will be invalid.
  void OpenFile(PP_Resource file_io,
                PP_Resource file_ref,
                PP_Resource file_system) {
    MockCompletionCallback cb;
    int32_t result = file_io_iface->Open(
        file_io,
        file_ref,
        PP_FILEOPENFLAG_WRITE,
        PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    // Should have sent an "open" message.
    ResourceMessageCallParams params;
    IPC::Message msg;
    ASSERT_TRUE(sink().GetFirstResourceCallMatching(
        PpapiHostMsg_FileIO_Open::ID, &params, &msg));
    sink().ClearMessages();

    // Send a success reply.
    ResourceMessageReplyParams reply_params(params.pp_resource(),
                                            params.sequence());
    reply_params.set_result(PP_OK);
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params,
        PpapiPluginMsg_FileIO_OpenReply(file_system,
                                        0 /* max_written_offset */));
  }
};

}  // namespace

// Test that Open fails if either host returns failure. The other tests exercise
// the case where both hosts return PP_OK.
TEST_F(FileSystemResourceTest, OpenFailure) {
  // Fail if the first reply doesn't return PP_OK.
  {
    LockingResourceReleaser file_system(
        file_system_iface->Create(pp_instance(),
                                  PP_FILESYSTEMTYPE_LOCALTEMPORARY));

    MockCompletionCallback cb;
    int32_t result = file_system_iface->Open(
        file_system.get(),
        kExpectedFileSystemSize,
        PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    ResourceMessageTestSink::ResourceCallVector open_messages =
        sink().GetAllResourceCallsMatching(PpapiHostMsg_FileSystem_Open::ID);
    ASSERT_EQ(2U, open_messages.size());
    sink().ClearMessages();

    SendOpenReply(open_messages[0].first, PP_ERROR_FAILED);
    SendOpenReply(open_messages[1].first, PP_OK);

    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  }
  // Fail if the second reply doesn't return PP_OK.
  {
    LockingResourceReleaser file_system(
        file_system_iface->Create(pp_instance(),
                                  PP_FILESYSTEMTYPE_LOCALTEMPORARY));

    MockCompletionCallback cb;
    int32_t result = file_system_iface->Open(
        file_system.get(),
        kExpectedFileSystemSize,
        PP_MakeCompletionCallback(&MockCompletionCallback::Callback, &cb));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

    ResourceMessageTestSink::ResourceCallVector open_messages =
        sink().GetAllResourceCallsMatching(PpapiHostMsg_FileSystem_Open::ID);
    ASSERT_EQ(2U, open_messages.size());
    sink().ClearMessages();

    SendOpenReply(open_messages[0].first, PP_OK);
    SendOpenReply(open_messages[1].first, PP_ERROR_FAILED);

    ASSERT_TRUE(cb.called());
    ASSERT_EQ(PP_ERROR_FAILED, cb.result());
  }
}

TEST_F(FileSystemResourceTest, RequestQuota) {
  LockingResourceReleaser file_system(
      file_system_iface->Create(pp_instance(),
                                PP_FILESYSTEMTYPE_LOCALTEMPORARY));

  OpenFileSystem(file_system.get());

  // Create and open two files in the file system. FileIOResource calls
  // FileSystemResource::OpenQuotaFile on success.
  LockingResourceReleaser file_ref1(
      file_ref_iface->Create(file_system.get(), "/file1"));
  LockingResourceReleaser file_io1(file_io_iface->Create(pp_instance()));
  OpenFile(file_io1.get(), file_ref1.get(), file_system.get());
  LockingResourceReleaser file_ref2(
      file_ref_iface->Create(file_system.get(), "/file2"));
  LockingResourceReleaser file_io2(file_io_iface->Create(pp_instance()));
  OpenFile(file_io2.get(), file_ref2.get(), file_system.get());

  EnterResource<PPB_FileSystem_API> enter(file_system.get(), true);
  ASSERT_FALSE(enter.failed());
  PPB_FileSystem_API* file_system_api = enter.object();

  MockRequestQuotaCallback cb1;
  int64_t result = file_system_api->RequestQuota(
      kQuotaRequestAmount1, base::BindOnce(&MockRequestQuotaCallback::Callback,
                                           base::Unretained(&cb1)));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  // Should have sent a "reserve quota" message, with the amount of the request
  // and a map of all currently open files to their max written offsets.
  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_FileSystem_ReserveQuota::ID, &params, &msg));
  sink().ClearMessages();

  int64_t amount = 0;
  FileGrowthMap file_growths;
  ASSERT_TRUE(UnpackMessage<PpapiHostMsg_FileSystem_ReserveQuota>(
      msg, &amount, &file_growths));
  ASSERT_EQ(kQuotaRequestAmount1, amount);
  ASSERT_EQ(2U, file_growths.size());
  ASSERT_EQ(0, file_growths[file_io1.get()].max_written_offset);
  ASSERT_EQ(0, file_growths[file_io2.get()].max_written_offset);

  // Make another request while the "reserve quota" message is pending.
  MockRequestQuotaCallback cb2;
  result = file_system_api->RequestQuota(
      kQuotaRequestAmount2, base::BindOnce(&MockRequestQuotaCallback::Callback,
                                           base::Unretained(&cb2)));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  // No new "reserve quota" message should be sent while one is pending.
  ASSERT_FALSE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_FileSystem_ReserveQuota::ID, &params, &msg));
  {
    ProxyAutoUnlock unlock_to_prevent_deadlock;
    // Reply with quota reservation amount sufficient to cover both requests.
    // Both callbacks should be called with the requests granted.
    SendReply(params,
              PP_OK,
              PpapiPluginMsg_FileSystem_ReserveQuotaReply(
                  kQuotaRequestAmount1 + kQuotaRequestAmount2,
                  FileGrowthMapToFileSizeMapForTesting(file_growths)));
  }
  ASSERT_TRUE(cb1.called());
  ASSERT_EQ(kQuotaRequestAmount1, cb1.result());
  ASSERT_TRUE(cb2.called());
  ASSERT_EQ(kQuotaRequestAmount2, cb2.result());
  cb1.Reset();
  cb2.Reset();

  // All requests should fail when insufficient quota is returned to satisfy
  // the first request.
  result = file_system_api->RequestQuota(
      kQuotaRequestAmount1, base::BindOnce(&MockRequestQuotaCallback::Callback,
                                           base::Unretained(&cb1)));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = file_system_api->RequestQuota(
      kQuotaRequestAmount2, base::BindOnce(&MockRequestQuotaCallback::Callback,
                                           base::Unretained(&cb2)));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_FileSystem_ReserveQuota::ID, &params, &msg));
  sink().ClearMessages();
  {
    ProxyAutoUnlock unlock_to_prevent_deadlock;
    // Reply with quota reservation amount insufficient to cover the first
    // request.
    SendReply(params,
              PP_OK,
              PpapiPluginMsg_FileSystem_ReserveQuotaReply(
                  kQuotaRequestAmount1 - 1,
                  FileGrowthMapToFileSizeMapForTesting(file_growths)));
  }
  ASSERT_TRUE(cb1.called());
  ASSERT_EQ(0, cb1.result());
  ASSERT_TRUE(cb2.called());
  ASSERT_EQ(0, cb2.result());
  cb1.Reset();
  cb2.Reset();

  // A new request should be made if the quota reservation is enough to satisfy
  // at least one request.
  result = file_system_api->RequestQuota(
      kQuotaRequestAmount1, base::BindOnce(&MockRequestQuotaCallback::Callback,
                                           base::Unretained(&cb1)));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = file_system_api->RequestQuota(
      kQuotaRequestAmount2, base::BindOnce(&MockRequestQuotaCallback::Callback,
                                           base::Unretained(&cb2)));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_FileSystem_ReserveQuota::ID, &params, &msg));
  sink().ClearMessages();
  {
    ProxyAutoUnlock unlock_to_prevent_deadlock;
    // Reply with quota reservation amount sufficient only to cover the first
    // request.
    SendReply(params,
              PP_OK,
              PpapiPluginMsg_FileSystem_ReserveQuotaReply(
                  kQuotaRequestAmount1,
                  FileGrowthMapToFileSizeMapForTesting(file_growths)));
  }
  ASSERT_TRUE(cb1.called());
  ASSERT_EQ(kQuotaRequestAmount1, cb1.result());
  ASSERT_FALSE(cb2.called());

  // Another request message should have been sent.
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_FileSystem_ReserveQuota::ID, &params, &msg));
  sink().ClearMessages();
  {
    ProxyAutoUnlock unlock_to_prevent_deadlock;
    // Reply with quota reservation amount sufficient to cover the second
    // request and some extra.
    SendReply(params,
              PP_OK,
              PpapiPluginMsg_FileSystem_ReserveQuotaReply(
                  kQuotaRequestAmount1 + kQuotaRequestAmount2,
                  FileGrowthMapToFileSizeMapForTesting(file_growths)));
  }

  ASSERT_TRUE(cb2.called());
  ASSERT_EQ(kQuotaRequestAmount2, cb2.result());
  cb1.Reset();
  cb2.Reset();

  // There is kQuotaRequestAmount1 of quota left, and a request for it should
  // succeed immediately.
  result = file_system_api->RequestQuota(
      kQuotaRequestAmount1, base::BindOnce(&MockRequestQuotaCallback::Callback,
                                           base::Unretained(&cb1)));
  ASSERT_EQ(kQuotaRequestAmount1, result);
}

}  // namespace proxy
}  // namespace ppapi
