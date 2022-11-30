// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_JSFS_JS_FS_H_
#define LIBRARIES_NACL_IO_JSFS_JS_FS_H_

#include <pthread.h>
#include <stdarg.h>

#include <map>

#include <ppapi/c/pp_var.h>

#include "nacl_io/filesystem.h"
#include "nacl_io/node.h"

namespace nacl_io {

class MessagingInterface;
class VarArrayBufferInterface;
class VarArrayInterface;
class VarDictionaryInterface;
class VarInterface;
class ScopedVar;

class JsFs : public Filesystem {
 public:
  typedef uint32_t RequestId;

  JsFs(const JsFs&) = delete;
  JsFs& operator=(const JsFs&) = delete;

 protected:
  JsFs();

  virtual Error Init(const FsInitArgs& args);
  virtual void Destroy();

 public:
  virtual Error OpenWithMode(const Path& path, int open_flags, mode_t mode,
                             ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int perm);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);
  virtual Error Filesystem_VIoctl(int request, va_list args);

 private:
  bool SetDictVar(PP_Var dict, const char* key, PP_Var value);
  PP_Var GetDictVar(PP_Var dict, const char* key);
  bool GetVarInt32(PP_Var var, int32_t* out_value);
  bool GetVarUint32(PP_Var var, uint32_t* out_value);
  bool GetVarInt64(PP_Var var, int64_t* out_value);

  PP_Var VMakeRequest(RequestId request_id, const char* format, va_list args);
  uint32_t VSendRequest(const char* format, va_list args);
  bool VSendRequestAndWait(ScopedVar* out_response,
                           const char* format,
                           va_list args);
  PP_Var WaitForResponse(uint32_t request_id);
  bool SendRequestAndWait(ScopedVar* out_response, const char* format, ...);
  Error ErrorFromResponse(const ScopedVar& response);

  int ScanVar(PP_Var var, const char* format, ...);
  int VScanVar(PP_Var var, const char* format, va_list args);

 private:
  MessagingInterface* messaging_iface_;
  VarArrayInterface* array_iface_;
  VarArrayBufferInterface* buffer_iface_;
  VarDictionaryInterface* dict_iface_;
  VarInterface* var_iface_;

  typedef std::map<RequestId, PP_Var> ResponseMap_t;
  sdk_util::SimpleLock lock_;
  RequestId request_id_;  // Protected by lock_;
  pthread_cond_t response_cond_;  // protected by lock_.
  ResponseMap_t responses_;       // protected by lock_.

  friend class JsFsNode;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_JSFS_JS_FS_H_
