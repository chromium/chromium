// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_NACL_MESSAGE_SCANNER_H_
#define PPAPI_PROXY_NACL_MESSAGE_SCANNER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/synchronization/lock.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class SerializedHandle;

class PPAPI_PROXY_EXPORT NaClMessageScanner {
 public:
  NaClMessageScanner();

  NaClMessageScanner(const NaClMessageScanner&) = delete;
  NaClMessageScanner& operator=(const NaClMessageScanner&) = delete;

  ~NaClMessageScanner();

  // Scans the message for items that require special handling. Copies any
  // SerializedHandles in the message into |handles| and if the message must be
  // rewritten for NaCl, sets |new_msg_ptr| to the new message. If no handles
  // are found, |handles| is left unchanged. If no rewriting is needed,
  // |new_msg_ptr| is left unchanged.
  //
  // For normal messages, |type| is equivalent to |msg|.id(), but, if |msg| is
  // a reply to a synchronous message, |type| is the id of the original
  // message.
  //
  // See more explanation in the method definition.
  //
  // See chrome/nacl/nacl_ipc_adapter.cc for where this is used to help convert
  // native handles to NaClDescs.
  bool ScanMessage(const IPC::Message& msg,
                   uint32_t type,
                   std::vector<SerializedHandle>* handles,
                   std::unique_ptr<IPC::Message>* new_msg_ptr);

  // Scans an untrusted message for items that require special handling. If the
  // message had to be rewritten, sets |new_msg_ptr| to the new message.
  void ScanUntrustedMessage(const IPC::Message& untrusted_msg,
                            std::unique_ptr<IPC::Message>* new_msg_ptr);

  // FileSystem information for quota auditing.
  class PPAPI_PROXY_EXPORT FileSystem {
   public:
    FileSystem();

    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;

    ~FileSystem();

    int64_t reserved_quota() const { return reserved_quota_; }

    // Adds amount to reserved quota. Returns true if reserved quota >= 0.
    bool UpdateReservedQuota(int64_t delta);

   private:
    base::Lock lock_;
    // This is the remaining amount of quota reserved for the file system.
    // Acquire the lock to modify this field, since it may be used on multiple
    // threads.
    int64_t reserved_quota_;
  };

  // FileIO information for quota auditing.
  class PPAPI_PROXY_EXPORT FileIO {
   public:
    FileIO(FileSystem* file_system, int64_t max_written_offset);

    FileIO(const FileIO&) = delete;
    FileIO& operator=(const FileIO&) = delete;

    ~FileIO();

    int64_t max_written_offset() { return max_written_offset_; }

    void SetMaxWrittenOffset(int64_t max_written_offset);

    // Grows file by the given amount. Returns true on success.
    bool Grow(int64_t amount);

   private:
    base::Lock lock_;

    // The file system that contains this file.
    FileSystem* file_system_;

    // The maximum written offset. This is initialized by NaClMessageScanner
    // when the file is opened and modified by a NaClDescQuotaInterface when the
    // plugin writes to greater maximum offsets.
    int64_t max_written_offset_;
  };

  FileIO* GetFile(PP_Resource file_io);

 private:
  friend class NaClMessageScannerTest;
  void AuditNestedMessage(PP_Resource resource,
                          const IPC::Message& msg,
                          SerializedHandle* handle);

  // We intercept FileSystem and FileIO messages to maintain information about
  // file systems and open files. This is used by NaClQuotaDescs to calculate
  // quota consumption and check it against the reserved amount.
  typedef std::map<int32_t, FileSystem*> FileSystemMap;
  FileSystemMap file_systems_;
  typedef std::map<int32_t, FileIO*> FileIOMap;
  FileIOMap files_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_NACL_MESSAGE_SCANNER_H_
