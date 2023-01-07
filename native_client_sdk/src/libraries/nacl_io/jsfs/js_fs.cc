// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/jsfs/js_fs.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>

#include "nacl_io/ioctl.h"
#include "nacl_io/jsfs/js_fs_node.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/log.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

namespace nacl_io {

JsFs::JsFs()
    : messaging_iface_(NULL),
      array_iface_(NULL),
      buffer_iface_(NULL),
      dict_iface_(NULL),
      var_iface_(NULL),
      request_id_(0) {
}

Error JsFs::Init(const FsInitArgs& args) {
  Error error = Filesystem::Init(args);
  if (error)
    return error;

  pthread_cond_init(&response_cond_, NULL);

  messaging_iface_ = ppapi_->GetMessagingInterface();
  array_iface_ = ppapi_->GetVarArrayInterface();
  buffer_iface_ = ppapi_->GetVarArrayBufferInterface();
  dict_iface_ = ppapi_->GetVarDictionaryInterface();
  var_iface_ = ppapi_->GetVarInterface();

  if (!messaging_iface_ || !array_iface_ || !buffer_iface_ || !dict_iface_ ||
      !var_iface_) {
    LOG_ERROR("Got 1+ NULL interface(s): %s%s%s%s%s",
              messaging_iface_ ? "" : "Messaging ",
              array_iface_ ? "" : "VarArray ",
              buffer_iface_ ? "" : "VarArrayBuffer ",
              dict_iface_ ? "" : "VarDictionary ",
              var_iface_ ? "" : "Var ");
    return ENOSYS;
  }

  return 0;
}

void JsFs::Destroy() {
  pthread_cond_destroy(&response_cond_);
}

bool JsFs::SetDictVar(PP_Var dict, const char* key, PP_Var value) {
  PP_Var key_var = var_iface_->VarFromUtf8(key, strlen(key));
  ScopedVar scoped_key(ppapi_, key_var);
  if (key_var.type != PP_VARTYPE_STRING) {
    LOG_ERROR("Unable to create string key \"%s\".", key);
    return false;
  }

  PP_Bool success = dict_iface_->Set(dict, key_var, value);
  if (!success) {
    LOG_ERROR("Unable to set \"%s\" key of dictionary.", key);
    return false;
  }

  return true;
}

PP_Var JsFs::GetDictVar(PP_Var dict, const char* key) {
  PP_Var key_var = var_iface_->VarFromUtf8(key, strlen(key));
  ScopedVar scoped_key(ppapi_, key_var);
  if (key_var.type != PP_VARTYPE_STRING) {
    LOG_ERROR("Unable to create string key \"%s\".", key);
    return PP_MakeUndefined();
  }

  return dict_iface_->Get(dict, key_var);
}

bool JsFs::GetVarInt32(PP_Var var, int32_t* out_value) {
  switch (var.type) {
    case PP_VARTYPE_INT32:
      *out_value = var.value.as_int;
      return true;

    case PP_VARTYPE_DOUBLE:
      *out_value = static_cast<int32_t>(var.value.as_double);
      return true;

    default:
      return false;
  }
}

bool JsFs::GetVarUint32(PP_Var var, uint32_t* out_value) {
  switch (var.type) {
    case PP_VARTYPE_INT32:
      *out_value = static_cast<uint32_t>(var.value.as_int);
      return true;

    case PP_VARTYPE_DOUBLE:
      *out_value = static_cast<uint32_t>(var.value.as_double);
      return true;

    default:
      return false;
  }
}

bool JsFs::GetVarInt64(PP_Var var, int64_t* out_value) {
  switch (var.type) {
    case PP_VARTYPE_INT32:
      *out_value = var.value.as_int;
      return true;

    case PP_VARTYPE_DOUBLE:
      *out_value = static_cast<int64_t>(var.value.as_double);
      return true;

    case PP_VARTYPE_ARRAY: {
      uint32_t len = array_iface_->GetLength(var);
      if (len != 2) {
        LOG_ERROR("Expected int64 array type to have 2 elements, not %d", len);
        return false;
      }

      PP_Var high_int_var = array_iface_->Get(var, 0);
      ScopedVar scoped_high_int_var(ppapi_, high_int_var);
      uint32_t high_int;
      if (!GetVarUint32(high_int_var, &high_int))
        return false;

      PP_Var low_int_var = array_iface_->Get(var, 1);
      ScopedVar scoped_low_int_var(ppapi_, low_int_var);
      uint32_t low_int;
      if (!GetVarUint32(low_int_var, &low_int))
        return false;

      *out_value = static_cast<int64_t>(
          (static_cast<uint64_t>(high_int) << 32) | low_int);
      return true;
    }

    default:
      return false;
  }
}

PP_Var JsFs::VMakeRequest(RequestId request_id,
                          const char* format,
                          va_list args) {
  PP_Var dict = dict_iface_->Create();
  ScopedVar scoped_dict(ppapi_, dict);

  if (!SetDictVar(dict, "id", PP_MakeInt32(request_id)))
    return PP_MakeNull();

  const char* p = format;
  while (*p) {
    assert(*p == '%');
    ++p;

    const char* key = va_arg(args, const char*);
    PP_Var value_var = PP_MakeUndefined();

    switch(*p) {
      case 'd':
        value_var = PP_MakeInt32(va_arg(args, int32_t));
        break;
      case 'u':
        value_var = PP_MakeInt32(va_arg(args, uint32_t));
        break;
      case 's': {
        const char* value = va_arg(args, const char*);
        value_var = var_iface_->VarFromUtf8(value, strlen(value));
        if (value_var.type != PP_VARTYPE_STRING) {
          LOG_ERROR("Unable to create \"%s\" string var.", value);
          return PP_MakeNull();
        }
        break;
      }
      case 'p':
        value_var = *va_arg(args, const PP_Var*);
        var_iface_->AddRef(value_var);
        break;
      case 'l': {
        // Only '%lld' is supported.
        ++p;
        assert(*p == 'l');
        ++p;
        assert(*p == 'd');

        int64_t value = va_arg(args, int64_t);
        if (value >= INT_MIN && value <= INT_MAX) {
          // Send as an int.
          value_var = PP_MakeInt32(static_cast<int32_t>(value));
        } else {
          // Send as an array of two ints: [high int32, low int32].
          value_var = array_iface_->Create();
          if (!array_iface_->SetLength(value_var, 2)) {
            LOG_ERROR("Unable to set length of s64 array.");
            return PP_MakeNull();
          }

          if (!array_iface_->Set(value_var, 0, PP_MakeInt32(value >> 32))) {
            LOG_ERROR("Unable to set of high int32 of s64 array.");
            return PP_MakeNull();
          }

          if (!array_iface_->Set(
                  value_var, 1, PP_MakeInt32(value & 0xffffffff))) {
            LOG_ERROR("Unable to set of low int32 of s64 array.");
            return PP_MakeNull();
          }
        }

        break;
      }
      default:
        LOG_ERROR("Unknown format specifier %%\"%s\"", p);
        assert(0);
        return PP_MakeNull();
    }

    ++p;

    if (!SetDictVar(dict, key, value_var))
      return PP_MakeNull();

    // Unconditionally release the value var. It is legal to do this even for
    // non-refcounted types.
    var_iface_->Release(value_var);
  }

  return scoped_dict.Release();
}

JsFs::RequestId JsFs::VSendRequest(const char* format, va_list args) {
  AUTO_LOCK(lock_);
  RequestId id = ++request_id_;
  // Skip 0 (the invalid request id) in the very unlikely case that the request
  // id wraps.
  if (id == 0)
    id = ++request_id_;

  PP_Var dict_var = VMakeRequest(id, format, args);
  ScopedVar scoped_dict_var(ppapi_, dict_var);
  if (dict_var.type != PP_VARTYPE_DICTIONARY)
    return 0;

  messaging_iface_->PostMessage(ppapi_->GetInstance(), dict_var);
  return id;
}

bool JsFs::VSendRequestAndWait(ScopedVar* out_response,
                               const char* format,
                               va_list args) {
  RequestId id = VSendRequest(format, args);
  if (id == 0)
    return false;

  out_response->Reset(WaitForResponse(id));
  return true;
}

bool JsFs::SendRequestAndWait(ScopedVar* out_response,
                              const char* format,
                              ...) {
  va_list args;
  va_start(args, format);
  bool result = VSendRequestAndWait(out_response, format, args);
  va_end(args);
  return result;
}

Error JsFs::ErrorFromResponse(const ScopedVar& response) {
  int32_t error;
  if (ScanVar(response.pp_var(), "%d", "error", &error) != 1) {
    LOG_ERROR("Expected \"error\" field in response.");
    return EINVAL;
  }

  return error;
}

int JsFs::ScanVar(PP_Var var, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VScanVar(var, format, args);
  va_end(args);
  return result;
}

int JsFs::VScanVar(PP_Var dict_var, const char* format, va_list args) {
  if (dict_var.type != PP_VARTYPE_DICTIONARY) {
    LOG_ERROR("Expected var of type dictionary, not %d.", dict_var.type);
    return 0;
  }

  int num_values = 0;

  const char* p = format;
  while (*p) {
    assert(*p == '%');
    ++p;

    const char* key = va_arg(args, const char*);
    PP_Var value_var = GetDictVar(dict_var, key);
    ScopedVar scoped_value_var(ppapi_, value_var);

    if (value_var.type == PP_VARTYPE_UNDEFINED)
      break;

    bool ok = true;

    switch (*p) {
      case 'd': {
        int32_t* value = va_arg(args, int32_t*);
        if (!GetVarInt32(value_var, value)) {
          LOG_ERROR("Expected int32_t value for key \"%s\" (got %d)", key,
                    value_var.type);
          ok = false;
        }
        break;
      }
      case 'h': {
        // Only '%hd' is supported.
        ++p;
        assert(*p == 'd');
        // Read 32-bit value from Pepper and truncate to 16-bits.
        int32_t value = 0;
        if (!GetVarInt32(value_var, &value)) {
          LOG_ERROR("Expected int32_t value for key \"%s\" (got %d)", key,
                    value_var.type);
          ok = false;
        }
        int16_t* short_value = va_arg(args, int16_t*);
        *short_value = (int16_t)value;
        break;
      }
      case 'u': {
        uint32_t* value = va_arg(args, uint32_t*);
        if (!GetVarUint32(value_var, value)) {
          LOG_ERROR("Expected uint32_t value for key \"%s\"", key);
          ok = false;
        }
        break;
      }
      case 'l': {
        // Only '%lld' is supported.
        ++p;
        assert(*p == 'l');
        ++p;
        assert(*p == 'd');

        int64_t* value = va_arg(args, int64_t*);
        if (!GetVarInt64(value_var, value)) {
          LOG_ERROR("Expected int64_t value for key \"%s\"", key);
          ok = false;
        }
        break;
      }
      case 'p': {
        PP_Var* value = va_arg(args, PP_Var*);
        *value = scoped_value_var.Release();
        break;
      }
      default:
        LOG_ERROR("Unknown format specifier %%\"%s\"", p);
        assert(0);
        ok = false;
        break;
    }

    if (!ok)
      break;

    p++;
    num_values++;
  }

  return num_values;
}

PP_Var JsFs::WaitForResponse(RequestId request_id) {
  AUTO_LOCK(lock_);
  while (1) {
    ResponseMap_t::iterator iter = responses_.find(request_id);
    if (iter != responses_.end()) {
      PP_Var response = iter->second;
      responses_.erase(iter);
      return response;
    }

    pthread_cond_wait(&response_cond_, lock_.mutex());
  }
}

Error JsFs::OpenWithMode(const Path& path, int open_flags, mode_t t,
                         ScopedNode* out_node) {
  out_node->reset(NULL);
  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%s%d",
                          "cmd", "open",
                          "path", path.Join().c_str(),
                          "oflag", open_flags)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  int32_t error;
  int32_t fd;
  int result = ScanVar(response.pp_var(), "%d%d", "error", &error, "fd", &fd);
  if (result >= 1 && error)
    return error;

  if (result != 2) {
    LOG_ERROR("Expected \"error\" and \"fd\" fields in response.");
    return EINVAL;
  }

  out_node->reset(new JsFsNode(this, fd));
  return 0;
}

Error JsFs::Unlink(const Path& path) {
  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(
          &response, "%s%s", "cmd", "unlink", "path", path.Join().c_str())) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  return ErrorFromResponse(response);
}

Error JsFs::Mkdir(const Path& path, int perm) {
  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%s%d",
                          "cmd", "mkdir",
                          "path", path.Join().c_str(),
                          "mode", perm)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  return ErrorFromResponse(response);
}

Error JsFs::Rmdir(const Path& path) {
  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(
          &response, "%s%s", "cmd", "rmdir", "path", path.Join().c_str())) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  return ErrorFromResponse(response);
}

Error JsFs::Remove(const Path& path) {
  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(
          &response, "%s%s", "cmd", "remove", "path", path.Join().c_str())) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  return ErrorFromResponse(response);
}

Error JsFs::Rename(const Path& path, const Path& newpath) {
  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%s%s",
                          "cmd", "rename",
                          "old", path.Join().c_str(),
                          "new", newpath.Join().c_str())) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  return ErrorFromResponse(response);
}

Error JsFs::Filesystem_VIoctl(int request, va_list args) {
  if (request != NACL_IOC_HANDLEMESSAGE) {
    LOG_ERROR("Unknown ioctl: %#x", request);
    return EINVAL;
  }

  PP_Var response = *va_arg(args, PP_Var*);

  AUTO_LOCK(lock_);

  RequestId response_id;
  if (ScanVar(response, "%d", "id", &response_id) != 1) {
    LOG_TRACE("ioctl with no \"id\", ignoring.\n");
    return EINVAL;
  }

  responses_.insert(ResponseMap_t::value_type(response_id, response));
  pthread_cond_broadcast(&response_cond_);
  return 0;
}

}  // namespace nacl_io
