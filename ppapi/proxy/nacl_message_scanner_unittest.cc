// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ipc/ipc_message.h"
#include "ppapi/proxy/nacl_message_scanner.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

namespace {
const PP_Resource kInvalidResource = 0;
const PP_Resource kFileSystem = 1;
const PP_Resource kFileIO = 2;
const int64_t kQuotaReservationAmount = 100;
}

class NaClMessageScannerTest : public PluginProxyTest {
 public:
  NaClMessageScannerTest() {}

  NaClMessageScanner::FileSystem* FindFileSystem(
      const NaClMessageScanner& scanner,
      PP_Resource file_system) {
    NaClMessageScanner::FileSystemMap::const_iterator it =
        scanner.file_systems_.find(file_system);
    return (it != scanner.file_systems_.end()) ? it->second : NULL;
  }

  NaClMessageScanner::FileIO* FindFileIO(
      const NaClMessageScanner& scanner,
      PP_Resource file_io) {
    NaClMessageScanner::FileIOMap::const_iterator it =
        scanner.files_.find(file_io);
    return (it != scanner.files_.end()) ? it->second : NULL;
  }

  void OpenQuotaFile(NaClMessageScanner* scanner,
                     PP_Resource file_io,
                     PP_Resource file_system) {
    std::vector<SerializedHandle> unused_handles;
    ResourceMessageReplyParams fio_reply_params(file_io, 0);
    std::unique_ptr<IPC::Message> new_msg_ptr;
    scanner->ScanMessage(
        PpapiPluginMsg_ResourceReply(
            fio_reply_params,
            PpapiPluginMsg_FileIO_OpenReply(file_system, 0)),
        PpapiPluginMsg_ResourceReply::ID,
        &unused_handles,
        &new_msg_ptr);
    EXPECT_FALSE(new_msg_ptr);
  }
};

TEST_F(NaClMessageScannerTest, FileOpenClose) {
  NaClMessageScanner test;
  std::vector<SerializedHandle> unused_handles;
  ResourceMessageCallParams fio_call_params(kFileIO, 0);
  ResourceMessageCallParams fs_call_params(kFileSystem, 0);
  ResourceMessageReplyParams fio_reply_params(kFileIO, 0);
  ResourceMessageReplyParams fs_reply_params(kFileSystem, 0);
  std::unique_ptr<IPC::Message> new_msg_ptr;

  EXPECT_EQ(NULL, FindFileSystem(test, kFileSystem));
  EXPECT_EQ(NULL, FindFileIO(test, kFileIO));

  // Open a file, not in a quota file system.
  test.ScanMessage(
      PpapiPluginMsg_ResourceReply(
          fio_reply_params,
          PpapiPluginMsg_FileIO_OpenReply(kInvalidResource, 0)),
      PpapiPluginMsg_ResourceReply::ID,
      &unused_handles,
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_FALSE(FindFileSystem(test, kFileSystem));
  EXPECT_FALSE(FindFileIO(test, kFileIO));

  // Open a file in a quota file system; info objects for it and its file system
  // should be created.
  OpenQuotaFile(&test, kFileIO, kFileSystem);
  NaClMessageScanner::FileSystem* fs = FindFileSystem(test, kFileSystem);
  NaClMessageScanner::FileIO* fio = FindFileIO(test, kFileIO);
  EXPECT_TRUE(fs);
  EXPECT_EQ(0, fs->reserved_quota());
  EXPECT_TRUE(fio);
  EXPECT_EQ(0, fio->max_written_offset());

  const int64_t kNewFileSize = 10;
  fio->SetMaxWrittenOffset(kNewFileSize);

  // We should not be able to under-report max_written_offset when closing.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_Close(FileGrowth(0, 0))),
      &new_msg_ptr);
  EXPECT_TRUE(new_msg_ptr);
  ResourceMessageCallParams call_params;
  IPC::Message nested_msg;
  FileGrowth file_growth;
  EXPECT_TRUE(UnpackMessage<PpapiHostMsg_ResourceCall>(
                  *new_msg_ptr, &call_params, &nested_msg) &&
              UnpackMessage<PpapiHostMsg_FileIO_Close>(
                  nested_msg, &file_growth));
  new_msg_ptr.reset();
  EXPECT_EQ(kNewFileSize, file_growth.max_written_offset);
  EXPECT_FALSE(FindFileIO(test, kFileIO));

  // Reopen the file.
  OpenQuotaFile(&test, kFileIO, kFileSystem);
  fio = FindFileIO(test, kFileIO);
  fio->SetMaxWrittenOffset(kNewFileSize);

  // Close with correct max_written_offset.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_Close(FileGrowth(kNewFileSize, 0))),
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_FALSE(FindFileIO(test, kFileIO));

  // Destroy file system.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fs_call_params,
          PpapiHostMsg_ResourceDestroyed(kFileSystem)),
      &new_msg_ptr);
  EXPECT_FALSE(FindFileSystem(test, kFileSystem));
}

TEST_F(NaClMessageScannerTest, QuotaAuditing) {
  NaClMessageScanner test;
  std::vector<SerializedHandle> unused_handles;
  ResourceMessageCallParams fio_call_params(kFileIO, 0);
  ResourceMessageCallParams fs_call_params(kFileSystem, 0);
  ResourceMessageReplyParams fio_reply_params(kFileIO, 0);
  ResourceMessageReplyParams fs_reply_params(kFileSystem, 0);
  std::unique_ptr<IPC::Message> new_msg_ptr;

  OpenQuotaFile(&test, kFileIO, kFileSystem);
  NaClMessageScanner::FileSystem* fs = FindFileSystem(test, kFileSystem);
  NaClMessageScanner::FileIO* fio = FindFileIO(test, kFileIO);
  EXPECT_TRUE(fs);
  EXPECT_EQ(0, fs->reserved_quota());
  EXPECT_TRUE(fio);
  EXPECT_EQ(0, fio->max_written_offset());

  // Without reserving quota, we should not be able to grow the file.
  EXPECT_FALSE(fio->Grow(1));
  EXPECT_EQ(0, fs->reserved_quota());
  EXPECT_EQ(0, fio->max_written_offset());

  // Receive reserved quota, and updated file sizes.
  const int64_t kNewFileSize = 10;
  FileSizeMap file_sizes;
  file_sizes[kFileIO] = kNewFileSize;
  test.ScanMessage(
      PpapiPluginMsg_ResourceReply(
          fs_reply_params,
          PpapiPluginMsg_FileSystem_ReserveQuotaReply(
              kQuotaReservationAmount,
              file_sizes)),
      PpapiPluginMsg_ResourceReply::ID,
      &unused_handles,
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_EQ(kQuotaReservationAmount, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize, fio->max_written_offset());

  // We should be able to grow the file within quota.
  EXPECT_TRUE(fio->Grow(1));
  EXPECT_EQ(kQuotaReservationAmount - 1, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize + 1, fio->max_written_offset());

  // We should not be able to grow the file over quota.
  EXPECT_FALSE(fio->Grow(kQuotaReservationAmount));
  EXPECT_EQ(kQuotaReservationAmount - 1, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize + 1, fio->max_written_offset());

  // Plugin should not under-report max written offsets when reserving quota.
  file_sizes[kFileIO] = 0;  // should be kNewFileSize + 1.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileSystem_ReserveQuota(
              kQuotaReservationAmount,
              FileSizeMapToFileGrowthMapForTesting(file_sizes))),
      &new_msg_ptr);
  EXPECT_TRUE(new_msg_ptr);
  ResourceMessageCallParams call_params;
  IPC::Message nested_msg;
  int64_t amount = 0;
  FileGrowthMap new_file_growths;
  EXPECT_TRUE(UnpackMessage<PpapiHostMsg_ResourceCall>(
                  *new_msg_ptr, &call_params, &nested_msg) &&
              UnpackMessage<PpapiHostMsg_FileSystem_ReserveQuota>(
                  nested_msg, &amount, &new_file_growths));
  new_msg_ptr.reset();
  EXPECT_EQ(kQuotaReservationAmount, amount);
  EXPECT_EQ(kNewFileSize + 1, new_file_growths[kFileIO].max_written_offset);
}

TEST_F(NaClMessageScannerTest, SetLength) {
  NaClMessageScanner test;
  std::vector<SerializedHandle> unused_handles;
  ResourceMessageCallParams fio_call_params(kFileIO, 0);
  ResourceMessageCallParams fs_call_params(kFileSystem, 0);
  ResourceMessageReplyParams fio_reply_params(kFileIO, 0);
  ResourceMessageReplyParams fs_reply_params(kFileSystem, 0);
  std::unique_ptr<IPC::Message> new_msg_ptr;

  OpenQuotaFile(&test, kFileIO, kFileSystem);
  NaClMessageScanner::FileSystem* fs = FindFileSystem(test, kFileSystem);
  NaClMessageScanner::FileIO* fio = FindFileIO(test, kFileIO);

  // Receive reserved quota, and updated file sizes.
  const int64_t kNewFileSize = 10;
  FileSizeMap file_sizes;
  file_sizes[kFileIO] = 0;
  test.ScanMessage(
      PpapiPluginMsg_ResourceReply(
          fs_reply_params,
          PpapiPluginMsg_FileSystem_ReserveQuotaReply(
              kQuotaReservationAmount,
              file_sizes)),
      PpapiPluginMsg_ResourceReply::ID,
      &unused_handles,
      &new_msg_ptr);

  // We should be able to SetLength within quota.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_SetLength(kNewFileSize)),
      &new_msg_ptr);
  EXPECT_FALSE(new_msg_ptr);
  EXPECT_EQ(kQuotaReservationAmount - kNewFileSize, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize, fio->max_written_offset());

  // We shouldn't be able to SetLength beyond quota. The message should be
  // rewritten to fail with length == -1.
  test.ScanUntrustedMessage(
      PpapiHostMsg_ResourceCall(
          fio_call_params,
          PpapiHostMsg_FileIO_SetLength(kQuotaReservationAmount + 1)),
      &new_msg_ptr);
  EXPECT_TRUE(new_msg_ptr);
  ResourceMessageCallParams call_params;
  IPC::Message nested_msg;
  int64_t length = 0;
  EXPECT_TRUE(UnpackMessage<PpapiHostMsg_ResourceCall>(
                  *new_msg_ptr, &call_params, &nested_msg) &&
              UnpackMessage<PpapiHostMsg_FileIO_SetLength>(
                  nested_msg, &length));
  new_msg_ptr.reset();
  EXPECT_EQ(-1, length);
  EXPECT_EQ(kQuotaReservationAmount - kNewFileSize, fs->reserved_quota());
  EXPECT_EQ(kNewFileSize, fio->max_written_offset());
}

}  // namespace proxy
}  // namespace ppapi
