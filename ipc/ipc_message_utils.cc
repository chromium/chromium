// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_utils.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_mojo_param_traits.h"

#if defined(OS_MAC)
#include "ipc/mach_port_mac.h"
#endif

#if defined(OS_WIN)
#include <tchar.h>
#include "ipc/handle_win.h"
#include "ipc/ipc_platform_file.h"
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

#if defined(OS_FUCHSIA)
#include "base/fuchsia/fuchsia_logging.h"
#include "ipc/handle_attachment_fuchsia.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/scope_to_message_pipe.h"
#endif

namespace IPC {

namespace {

const int kMaxRecursionDepth = 200;

template<typename CharType>
void LogBytes(const std::vector<CharType>& data, std::string* out) {
#if defined(OS_WIN)
  // Windows has a GUI for logging, which can handle arbitrary binary data.
  for (size_t i = 0; i < data.size(); ++i)
    out->push_back(data[i]);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  // On POSIX, we log to stdout, which we assume can display ASCII.
  static const size_t kMaxBytesToLog = 100;
  for (size_t i = 0; i < std::min(data.size(), kMaxBytesToLog); ++i) {
    if (isprint(data[i]))
      out->push_back(data[i]);
    else
      out->append(
          base::StringPrintf("[%02X]", static_cast<unsigned char>(data[i])));
  }
  if (data.size() > kMaxBytesToLog) {
    out->append(base::StringPrintf(
        " and %u more bytes",
        static_cast<unsigned>(data.size() - kMaxBytesToLog)));
  }
#endif
}

bool ReadValue(const base::Pickle* m,
               base::PickleIterator* iter,
               base::Value* value,
               int recursion);

void WriteValue(base::Pickle* m, const base::Value* value, int recursion) {
  bool result;
  if (recursion > kMaxRecursionDepth) {
    LOG(ERROR) << "Max recursion depth hit in WriteValue.";
    return;
  }

  m->WriteInt(static_cast<int>(value->type()));

  switch (value->type()) {
    case base::Value::Type::NONE:
      break;
    case base::Value::Type::BOOLEAN: {
      bool val;
      result = value->GetAsBoolean(&val);
      DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case base::Value::Type::INTEGER: {
      int val;
      result = value->GetAsInteger(&val);
      DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case base::Value::Type::DOUBLE: {
      double val;
      result = value->GetAsDouble(&val);
      DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case base::Value::Type::STRING: {
      std::string val;
      result = value->GetAsString(&val);
      DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case base::Value::Type::BINARY: {
      m->WriteData(reinterpret_cast<const char*>(value->GetBlob().data()),
                   base::checked_cast<int>(value->GetBlob().size()));
      break;
    }
    case base::Value::Type::DICTIONARY: {
      const base::DictionaryValue* dict =
          static_cast<const base::DictionaryValue*>(value);

      WriteParam(m, base::checked_cast<int>(dict->size()));

      for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
           it.Advance()) {
        WriteParam(m, it.key());
        WriteValue(m, &it.value(), recursion + 1);
      }
      break;
    }
    case base::Value::Type::LIST: {
      const base::ListValue* list = static_cast<const base::ListValue*>(value);
      WriteParam(m, base::checked_cast<int>(list->GetSize()));
      for (const auto& entry : *list) {
        WriteValue(m, &entry, recursion + 1);
      }
      break;
    }

    // TODO(crbug.com/859477): Remove after root cause is found.
    default:
      CHECK(false);
      break;
  }
}

// Helper for ReadValue that reads a DictionaryValue into a pre-allocated
// object.
bool ReadDictionaryValue(const base::Pickle* m,
                         base::PickleIterator* iter,
                         base::DictionaryValue* value,
                         int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  std::vector<base::Value::LegacyDictStorage::value_type> entries;
  entries.resize(size);
  for (auto& entry : entries) {
    entry.second = std::make_unique<base::Value>();
    if (!ReadParam(m, iter, &entry.first) ||
        !ReadValue(m, iter, entry.second.get(), recursion + 1))
      return false;
  }

  *value =
      base::DictionaryValue(base::Value::LegacyDictStorage(std::move(entries)));
  return true;
}

// Helper for ReadValue that reads a ReadListValue into a pre-allocated
// object.
bool ReadListValue(const base::Pickle* m,
                   base::PickleIterator* iter,
                   base::ListValue* value,
                   int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  base::Value::ListStorage list_storage;
  list_storage.resize(size);
  for (base::Value& subval : list_storage) {
    if (!ReadValue(m, iter, &subval, recursion + 1))
      return false;
  }
  *value = base::ListValue(std::move(list_storage));
  return true;
}

bool ReadValue(const base::Pickle* m,
               base::PickleIterator* iter,
               base::Value* value,
               int recursion) {
  if (recursion > kMaxRecursionDepth) {
    LOG(ERROR) << "Max recursion depth hit in ReadValue.";
    return false;
  }

  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  constexpr int kMinValueType = static_cast<int>(base::Value::Type::NONE);
  constexpr int kMaxValueType = static_cast<int>(base::Value::Type::LIST);
  if (type > kMaxValueType || type < kMinValueType)
    return false;

  switch (static_cast<base::Value::Type>(type)) {
    case base::Value::Type::NONE:
      *value = base::Value();
      break;
    case base::Value::Type::BOOLEAN: {
      bool val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = base::Value(val);
      break;
    }
    case base::Value::Type::INTEGER: {
      int val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = base::Value(val);
      break;
    }
    case base::Value::Type::DOUBLE: {
      double val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = base::Value(val);
      break;
    }
    case base::Value::Type::STRING: {
      std::string val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = base::Value(std::move(val));
      break;
    }
    case base::Value::Type::BINARY: {
      base::span<const uint8_t> data;
      if (!iter->ReadData(&data))
        return false;
      *value = base::Value(data);
      break;
    }
    case base::Value::Type::DICTIONARY: {
      base::DictionaryValue val;
      if (!ReadDictionaryValue(m, iter, &val, recursion))
        return false;
      *value = std::move(val);
      break;
    }
    case base::Value::Type::LIST: {
      base::ListValue val;
      if (!ReadListValue(m, iter, &val, recursion))
        return false;
      *value = std::move(val);
      break;
    }
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

}  // namespace

// -----------------------------------------------------------------------------

LogData::LogData()
    : routing_id(0),
      type(0),
      sent(0),
      receive(0),
      dispatch(0) {
}

LogData::LogData(const LogData& other) = default;

LogData::~LogData() = default;

void ParamTraits<bool>::Log(const param_type& p, std::string* l) {
  l->append(p ? "true" : "false");
}

void ParamTraits<signed char>::Write(base::Pickle* m, const param_type& p) {
  m->WriteBytes(&p, sizeof(param_type));
}

bool ParamTraits<signed char>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    param_type* r) {
  const char* data;
  if (!iter->ReadBytes(&data, sizeof(param_type)))
    return false;
  memcpy(r, data, sizeof(param_type));
  return true;
}

void ParamTraits<signed char>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

void ParamTraits<unsigned char>::Write(base::Pickle* m, const param_type& p) {
  m->WriteBytes(&p, sizeof(param_type));
}

bool ParamTraits<unsigned char>::Read(const base::Pickle* m,
                                      base::PickleIterator* iter,
                                      param_type* r) {
  const char* data;
  if (!iter->ReadBytes(&data, sizeof(param_type)))
    return false;
  memcpy(r, data, sizeof(param_type));
  return true;
}

void ParamTraits<unsigned char>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

void ParamTraits<unsigned short>::Write(base::Pickle* m, const param_type& p) {
  m->WriteBytes(&p, sizeof(param_type));
}

bool ParamTraits<unsigned short>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  const char* data;
  if (!iter->ReadBytes(&data, sizeof(param_type)))
    return false;
  memcpy(r, data, sizeof(param_type));
  return true;
}

void ParamTraits<unsigned short>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

void ParamTraits<int>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

void ParamTraits<unsigned int>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_FUCHSIA) || (defined(OS_ANDROID) && defined(ARCH_CPU_64_BITS))
void ParamTraits<long>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

void ParamTraits<unsigned long>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}
#endif

void ParamTraits<long long>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

void ParamTraits<unsigned long long>::Log(const param_type& p, std::string* l) {
  l->append(base::NumberToString(p));
}

void ParamTraits<float>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("%e", p));
}

void ParamTraits<double>::Write(base::Pickle* m, const param_type& p) {
  m->WriteBytes(reinterpret_cast<const char*>(&p), sizeof(param_type));
}

bool ParamTraits<double>::Read(const base::Pickle* m,
                               base::PickleIterator* iter,
                               param_type* r) {
  const char *data;
  if (!iter->ReadBytes(&data, sizeof(*r))) {
    NOTREACHED();
    return false;
  }
  memcpy(r, data, sizeof(param_type));
  return true;
}

void ParamTraits<double>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("%e", p));
}


void ParamTraits<std::string>::Log(const param_type& p, std::string* l) {
  l->append(p);
}

void ParamTraits<base::string16>::Log(const param_type& p, std::string* l) {
  l->append(base::UTF16ToUTF8(p));
}

#if defined(OS_WIN) && defined(BASE_STRING16_IS_STD_U16STRING)
bool ParamTraits<std::wstring>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* r) {
  base::StringPiece16 piece16;
  if (!iter->ReadStringPiece16(&piece16))
    return false;

  *r = base::AsWString(piece16);
  return true;
}

void ParamTraits<std::wstring>::Log(const param_type& p, std::string* l) {
  l->append(base::WideToUTF8(p));
}
#endif

void ParamTraits<std::vector<char>>::Write(base::Pickle* m,
                                           const param_type& p) {
  if (p.empty()) {
    m->WriteData(NULL, 0);
  } else {
    m->WriteData(&p.front(), base::checked_cast<int>(p.size()));
  }
}

bool ParamTraits<std::vector<char>>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  const char *data;
  int data_size = 0;
  if (!iter->ReadData(&data, &data_size) || data_size < 0)
    return false;
  r->resize(data_size);
  if (data_size)
    memcpy(&r->front(), data, data_size);
  return true;
}

void ParamTraits<std::vector<char> >::Log(const param_type& p, std::string* l) {
  LogBytes(p, l);
}

void ParamTraits<std::vector<unsigned char>>::Write(base::Pickle* m,
                                                    const param_type& p) {
  if (p.empty()) {
    m->WriteData(NULL, 0);
  } else {
    m->WriteData(reinterpret_cast<const char*>(&p.front()),
                 base::checked_cast<int>(p.size()));
  }
}

bool ParamTraits<std::vector<unsigned char>>::Read(const base::Pickle* m,
                                                   base::PickleIterator* iter,
                                                   param_type* r) {
  const char *data;
  int data_size = 0;
  if (!iter->ReadData(&data, &data_size) || data_size < 0)
    return false;
  r->resize(data_size);
  if (data_size)
    memcpy(&r->front(), data, data_size);
  return true;
}

void ParamTraits<std::vector<unsigned char> >::Log(const param_type& p,
                                                   std::string* l) {
  LogBytes(p, l);
}

void ParamTraits<std::vector<bool>>::Write(base::Pickle* m,
                                           const param_type& p) {
  WriteParam(m, base::checked_cast<int>(p.size()));
  // Cast to bool below is required because libc++'s
  // vector<bool>::const_reference is different from bool, and we want to avoid
  // writing an extra specialization of ParamTraits for it.
  for (size_t i = 0; i < p.size(); i++)
    WriteParam(m, static_cast<bool>(p[i]));
}

bool ParamTraits<std::vector<bool>>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  int size;
  // ReadLength() checks for < 0 itself.
  if (!iter->ReadLength(&size))
    return false;
  r->resize(size);
  for (int i = 0; i < size; i++) {
    bool value;
    if (!ReadParam(m, iter, &value))
      return false;
    (*r)[i] = value;
  }
  return true;
}

void ParamTraits<std::vector<bool> >::Log(const param_type& p, std::string* l) {
  for (size_t i = 0; i < p.size(); ++i) {
    if (i != 0)
      l->push_back(' ');
    LogParam(static_cast<bool>(p[i]), l);
  }
}

void ParamTraits<base::DictionaryValue>::Write(base::Pickle* m,
                                               const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<base::DictionaryValue>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) ||
      type != static_cast<int>(base::Value::Type::DICTIONARY))
    return false;

  return ReadDictionaryValue(m, iter, r, 0);
}

void ParamTraits<base::DictionaryValue>::Log(const param_type& p,
                                             std::string* l) {
  std::string json;
  base::JSONWriter::Write(p, &json);
  l->append(json);
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
void ParamTraits<base::FileDescriptor>::Write(base::Pickle* m,
                                              const param_type& p) {
  // This serialization must be kept in sync with
  // nacl_message_scanner.cc:WriteHandle().
  const bool valid = p.fd >= 0;
  WriteParam(m, valid);

  if (!valid)
    return;

  if (p.auto_close) {
    if (!m->WriteAttachment(
            new internal::PlatformFileAttachment(base::ScopedFD(p.fd))))
      NOTREACHED();
  } else {
    if (!m->WriteAttachment(new internal::PlatformFileAttachment(p.fd)))
      NOTREACHED();
  }
}

bool ParamTraits<base::FileDescriptor>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  *r = base::FileDescriptor();

  bool valid;
  if (!ReadParam(m, iter, &valid))
    return false;

  if (!valid)
    return true;

  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!m->ReadAttachment(iter, &attachment))
    return false;

  if (static_cast<MessageAttachment*>(attachment.get())->GetType() !=
      MessageAttachment::Type::PLATFORM_FILE) {
    return false;
  }

  *r = base::FileDescriptor(
      static_cast<internal::PlatformFileAttachment*>(attachment.get())
          ->TakePlatformFile(),
      true);
  return true;
}

void ParamTraits<base::FileDescriptor>::Log(const param_type& p,
                                            std::string* l) {
  if (p.auto_close) {
    l->append(base::StringPrintf("FD(%d auto-close)", p.fd));
  } else {
    l->append(base::StringPrintf("FD(%d)", p.fd));
  }
}

void ParamTraits<base::ScopedFD>::Write(base::Pickle* m, const param_type& p) {
  // This serialization must be kept in sync with
  // nacl_message_scanner.cc:WriteHandle().
  const bool valid = p.is_valid();
  WriteParam(m, valid);

  if (!valid)
    return;

  if (!m->WriteAttachment(new internal::PlatformFileAttachment(
          std::move(const_cast<param_type&>(p))))) {
    NOTREACHED();
  }
}

bool ParamTraits<base::ScopedFD>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  r->reset();

  bool valid;
  if (!ReadParam(m, iter, &valid))
    return false;

  if (!valid)
    return true;

  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!m->ReadAttachment(iter, &attachment))
    return false;

  if (static_cast<MessageAttachment*>(attachment.get())->GetType() !=
      MessageAttachment::Type::PLATFORM_FILE) {
    return false;
  }

  *r = base::ScopedFD(
      static_cast<internal::PlatformFileAttachment*>(attachment.get())
          ->TakePlatformFile());
  return true;
}

void ParamTraits<base::ScopedFD>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("ScopedFD(%d)", p.get()));
}
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

#if defined(OS_WIN)
void ParamTraits<base::win::ScopedHandle>::Write(base::Pickle* m,
                                                 const param_type& p) {
  const bool valid = p.IsValid();
  WriteParam(m, valid);
  if (!valid)
    return;

  HandleWin handle(p.Get());
  WriteParam(m, handle);
}

bool ParamTraits<base::win::ScopedHandle>::Read(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                param_type* r) {
  r->Close();

  bool valid;
  if (!ReadParam(m, iter, &valid))
    return false;
  if (!valid)
    return true;

  HandleWin handle;
  if (!ReadParam(m, iter, &handle))
    return false;

  r->Set(handle.get_handle());
  return true;
}

void ParamTraits<base::win::ScopedHandle>::Log(const param_type& p,
                                               std::string* l) {
  l->append(base::StringPrintf("ScopedHandle(%p)", p.Get()));
}
#endif  // defined(OS_WIN)

#if defined(OS_FUCHSIA)
void ParamTraits<zx::vmo>::Write(base::Pickle* m, const param_type& p) {
  // This serialization must be kept in sync with
  // nacl_message_scanner.cc:WriteHandle().
  const bool valid = p.is_valid();
  WriteParam(m, valid);

  if (!valid)
    return;

  if (!m->WriteAttachment(new internal::HandleAttachmentFuchsia(
          std::move(const_cast<param_type&>(p))))) {
    NOTREACHED();
  }
}

bool ParamTraits<zx::vmo>::Read(const base::Pickle* m,
                                base::PickleIterator* iter,
                                param_type* r) {
  r->reset();

  bool valid;
  if (!ReadParam(m, iter, &valid))
    return false;

  if (!valid)
    return true;

  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!m->ReadAttachment(iter, &attachment))
    return false;

  if (static_cast<MessageAttachment*>(attachment.get())->GetType() !=
      MessageAttachment::Type::FUCHSIA_HANDLE) {
    return false;
  }

  *r = zx::vmo(static_cast<internal::HandleAttachmentFuchsia*>(attachment.get())
                   ->Take());
  return true;
}

void ParamTraits<zx::vmo>::Log(const param_type& p, std::string* l) {
  l->append("ZirconVMO");
}

void ParamTraits<zx::channel>::Write(base::Pickle* m, const param_type& p) {
  // This serialization must be kept in sync with
  // nacl_message_scanner.cc:WriteHandle().
  const bool valid = p.is_valid();
  WriteParam(m, valid);

  if (!valid)
    return;

  if (!m->WriteAttachment(new internal::HandleAttachmentFuchsia(
          std::move(const_cast<param_type&>(p))))) {
    NOTREACHED();
  }
}

bool ParamTraits<zx::channel>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    param_type* r) {
  r->reset();

  bool valid;
  if (!ReadParam(m, iter, &valid))
    return false;

  if (!valid)
    return true;

  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!m->ReadAttachment(iter, &attachment))
    return false;

  if (static_cast<MessageAttachment*>(attachment.get())->GetType() !=
      MessageAttachment::Type::FUCHSIA_HANDLE) {
    return false;
  }

  *r = zx::channel(
      static_cast<internal::HandleAttachmentFuchsia*>(attachment.get())
          ->Take());
  return true;
}

void ParamTraits<zx::channel>::Log(const param_type& p, std::string* l) {
  l->append("ZirconChannel");
}
#endif  // defined(OS_FUCHSIA)

#if defined(OS_ANDROID)
void ParamTraits<base::android::ScopedHardwareBufferHandle>::Write(
    base::Pickle* m,
    const param_type& p) {
  const bool is_valid = p.is_valid();
  WriteParam(m, is_valid);
  if (!is_valid)
    return;

  // We must keep a ref to the AHardwareBuffer alive until the receiver has
  // acquired its own reference. We do this by sending a message pipe handle
  // along with the buffer. When the receiver deserializes (or even if they
  // die without ever reading the message) their end of the pipe will be
  // closed. We will eventually detect this and release the AHB reference.
  mojo::MessagePipe tracking_pipe;
  m->WriteAttachment(new internal::MojoHandleAttachment(
      mojo::ScopedHandle::From(std::move(tracking_pipe.handle0))));
  WriteParam(m, base::FileDescriptor(p.SerializeAsFileDescriptor().release(),
                                     true /* auto_close */));

  // Pass ownership of the input handle to our tracking pipe to keep the AHB
  // alive long enough to be deserialized by the receiver.
  mojo::ScopeToMessagePipe(std::move(const_cast<param_type&>(p)),
                           std::move(tracking_pipe.handle1));
}

bool ParamTraits<base::android::ScopedHardwareBufferHandle>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  *r = base::android::ScopedHardwareBufferHandle();

  bool is_valid;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;

  scoped_refptr<base::Pickle::Attachment> tracking_pipe_attachment;
  if (!m->ReadAttachment(iter, &tracking_pipe_attachment))
    return false;

  // We keep this alive until the AHB is safely deserialized below. When this
  // goes out of scope, the sender holding the other end of this pipe will treat
  // this handle closure as a signal that it's safe to release their AHB
  // keepalive ref.
  mojo::ScopedHandle tracking_pipe =
      static_cast<MessageAttachment*>(tracking_pipe_attachment.get())
          ->TakeMojoHandle();

  base::FileDescriptor descriptor;
  if (!ReadParam(m, iter, &descriptor))
    return false;

  // NOTE: It is valid to deserialize an invalid FileDescriptor, so the success
  // of |ReadParam()| above does not imply that |descriptor| is valid.
  base::ScopedFD scoped_fd(descriptor.fd);
  if (!scoped_fd.is_valid())
    return false;

  *r = base::android::ScopedHardwareBufferHandle::DeserializeFromFileDescriptor(
      std::move(scoped_fd));
  return true;
}

void ParamTraits<base::android::ScopedHardwareBufferHandle>::Log(
    const param_type& p,
    std::string* l) {
  l->append(base::StringPrintf("base::android::ScopedHardwareBufferHandle(%p)",
                               p.get()));
}
#endif  // defined(OS_ANDROID)

void ParamTraits<base::ReadOnlySharedMemoryRegion>::Write(base::Pickle* m,
                                                          const param_type& p) {
  base::subtle::PlatformSharedMemoryRegion handle =
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(const_cast<param_type&>(p)));
  WriteParam(m, std::move(handle));
}

bool ParamTraits<base::ReadOnlySharedMemoryRegion>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  base::subtle::PlatformSharedMemoryRegion handle;
  if (!ReadParam(m, iter, &handle))
    return false;

  *r = base::ReadOnlySharedMemoryRegion::Deserialize(std::move(handle));
  return true;
}

void ParamTraits<base::ReadOnlySharedMemoryRegion>::Log(const param_type& p,
                                                        std::string* l) {
  *l = "<base::ReadOnlySharedMemoryRegion>";
  // TODO(alexilin): currently there is no way to access underlying handle
  // without destructing a ReadOnlySharedMemoryRegion instance.
}

void ParamTraits<base::WritableSharedMemoryRegion>::Write(base::Pickle* m,
                                                          const param_type& p) {
  base::subtle::PlatformSharedMemoryRegion handle =
      base::WritableSharedMemoryRegion::TakeHandleForSerialization(
          std::move(const_cast<param_type&>(p)));
  WriteParam(m, std::move(handle));
}

bool ParamTraits<base::WritableSharedMemoryRegion>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  base::subtle::PlatformSharedMemoryRegion handle;
  if (!ReadParam(m, iter, &handle))
    return false;

  *r = base::WritableSharedMemoryRegion::Deserialize(std::move(handle));
  return true;
}

void ParamTraits<base::WritableSharedMemoryRegion>::Log(const param_type& p,
                                                        std::string* l) {
  *l = "<base::WritableSharedMemoryRegion>";
  // TODO(alexilin): currently there is no way to access underlying handle
  // without destructing a ReadOnlySharedMemoryRegion instance.
}

void ParamTraits<base::UnsafeSharedMemoryRegion>::Write(base::Pickle* m,
                                                        const param_type& p) {
  base::subtle::PlatformSharedMemoryRegion handle =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(const_cast<param_type&>(p)));
  WriteParam(m, std::move(handle));
}

bool ParamTraits<base::UnsafeSharedMemoryRegion>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  base::subtle::PlatformSharedMemoryRegion handle;
  if (!ReadParam(m, iter, &handle))
    return false;

  *r = base::UnsafeSharedMemoryRegion::Deserialize(std::move(handle));
  return true;
}

void ParamTraits<base::UnsafeSharedMemoryRegion>::Log(const param_type& p,
                                                      std::string* l) {
  *l = "<base::UnsafeSharedMemoryRegion>";
  // TODO(alexilin): currently there is no way to access underlying handle
  // without destructing a ReadOnlySharedMemoryRegion instance.
}

void ParamTraits<base::subtle::PlatformSharedMemoryRegion>::Write(
    base::Pickle* m,
    const param_type& p) {
  // This serialization must be kept in sync with
  // nacl_message_scanner.cc::WriteHandle().
  const bool valid = p.IsValid();
  WriteParam(m, valid);

  if (!valid)
    return;

  WriteParam(m, p.GetMode());
  WriteParam(m, static_cast<uint64_t>(p.GetSize()));
  WriteParam(m, p.GetGUID());

#if defined(OS_WIN)
  base::win::ScopedHandle h = const_cast<param_type&>(p).PassPlatformHandle();
  HandleWin handle_win(h.Get());
  WriteParam(m, handle_win);
#elif defined(OS_FUCHSIA)
  zx::vmo vmo = const_cast<param_type&>(p).PassPlatformHandle();
  WriteParam(m, vmo);
#elif defined(OS_MAC)
  base::mac::ScopedMachSendRight h =
      const_cast<param_type&>(p).PassPlatformHandle();
  MachPortMac mach_port_mac(h.get());
  WriteParam(m, mach_port_mac);
#elif defined(OS_ANDROID)
  m->WriteAttachment(new internal::PlatformFileAttachment(
      base::ScopedFD(const_cast<param_type&>(p).PassPlatformHandle())));
#elif defined(OS_POSIX)
  base::subtle::ScopedFDPair h =
      const_cast<param_type&>(p).PassPlatformHandle();
  m->WriteAttachment(new internal::PlatformFileAttachment(std::move(h.fd)));
  if (p.GetMode() ==
      base::subtle::PlatformSharedMemoryRegion::Mode::kWritable) {
    m->WriteAttachment(
        new internal::PlatformFileAttachment(std::move(h.readonly_fd)));
  }
#endif
}

bool ParamTraits<base::subtle::PlatformSharedMemoryRegion>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool valid;
  if (!ReadParam(m, iter, &valid))
    return false;
  if (!valid) {
    *r = base::subtle::PlatformSharedMemoryRegion();
    return true;
  }

  base::subtle::PlatformSharedMemoryRegion::Mode mode;
  uint64_t shm_size;
  base::UnguessableToken guid;
  if (!ReadParam(m, iter, &mode) || !ReadParam(m, iter, &shm_size) ||
      !base::IsValueInRangeForNumericType<size_t>(shm_size) ||
      !ReadParam(m, iter, &guid)) {
    return false;
  }
  size_t size = static_cast<size_t>(shm_size);

#if defined(OS_WIN)
  HandleWin handle_win;
  if (!ReadParam(m, iter, &handle_win))
    return false;
  *r = base::subtle::PlatformSharedMemoryRegion::Take(
      base::win::ScopedHandle(handle_win.get_handle()), mode, size, guid);
#elif defined(OS_FUCHSIA)
  zx::vmo vmo;
  if (!ReadParam(m, iter, &vmo))
    return false;
  *r = base::subtle::PlatformSharedMemoryRegion::Take(std::move(vmo), mode,
                                                      size, guid);
#elif defined(OS_MAC)
  MachPortMac mach_port_mac;
  if (!ReadParam(m, iter, &mach_port_mac))
    return false;
  *r = base::subtle::PlatformSharedMemoryRegion::Take(
      base::mac::ScopedMachSendRight(mach_port_mac.get_mach_port()), mode, size,
      guid);
#elif defined(OS_POSIX)
  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!m->ReadAttachment(iter, &attachment))
    return false;
  if (static_cast<MessageAttachment*>(attachment.get())->GetType() !=
      MessageAttachment::Type::PLATFORM_FILE) {
    return false;
  }

#if defined(OS_ANDROID)
  *r = base::subtle::PlatformSharedMemoryRegion::Take(
      base::ScopedFD(
          static_cast<internal::PlatformFileAttachment*>(attachment.get())
              ->TakePlatformFile()),
      mode, size, guid);
#else
  scoped_refptr<base::Pickle::Attachment> readonly_attachment;
  if (mode == base::subtle::PlatformSharedMemoryRegion::Mode::kWritable) {
    if (!m->ReadAttachment(iter, &readonly_attachment))
      return false;

    if (static_cast<MessageAttachment*>(readonly_attachment.get())->GetType() !=
        MessageAttachment::Type::PLATFORM_FILE) {
      return false;
    }
  }
  *r = base::subtle::PlatformSharedMemoryRegion::Take(
      base::subtle::ScopedFDPair(
          base::ScopedFD(
              static_cast<internal::PlatformFileAttachment*>(attachment.get())
                  ->TakePlatformFile()),
          readonly_attachment
              ? base::ScopedFD(static_cast<internal::PlatformFileAttachment*>(
                                   readonly_attachment.get())
                                   ->TakePlatformFile())
              : base::ScopedFD()),
      mode, size, guid);
#endif  // defined(OS_ANDROID)

#endif

  return true;
}

void ParamTraits<base::subtle::PlatformSharedMemoryRegion>::Log(
    const param_type& p,
    std::string* l) {
#if defined(OS_FUCHSIA)
  l->append("Handle: ");
  LogParam(p.GetPlatformHandle()->get(), l);
#elif defined(OS_WIN)
  l->append("Handle: ");
  LogParam(p.GetPlatformHandle(), l);
#elif defined(OS_MAC)
  l->append("Mach port: ");
  LogParam(p.GetPlatformHandle(), l);
#elif defined(OS_ANDROID)
  l->append("FD: ");
  LogParam(p.GetPlatformHandle(), l);
#elif defined(OS_POSIX)
  base::subtle::FDPair h = p.GetPlatformHandle();
  l->append("FD: ");
  LogParam(h.fd, l);
  l->append("Read-only FD: ");
  LogParam(h.readonly_fd, l);
#endif

  l->append("Mode: ");
  LogParam(p.GetMode(), l);
  l->append("size: ");
  LogParam(static_cast<uint64_t>(p.GetSize()), l);
  l->append("GUID: ");
  LogParam(p.GetGUID(), l);
}

void ParamTraits<base::subtle::PlatformSharedMemoryRegion::Mode>::Write(
    base::Pickle* m,
    const param_type& value) {
  DCHECK(static_cast<int>(value) >= 0 &&
         static_cast<int>(value) <= static_cast<int>(param_type::kMaxValue));
  m->WriteInt(static_cast<int>(value));
}

bool ParamTraits<base::subtle::PlatformSharedMemoryRegion::Mode>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  int value;
  if (!iter->ReadInt(&value))
    return false;
  if (!(static_cast<int>(value) >= 0 &&
        static_cast<int>(value) <= static_cast<int>(param_type::kMaxValue))) {
    return false;
  }
  *p = static_cast<param_type>(value);
  return true;
}

void ParamTraits<base::subtle::PlatformSharedMemoryRegion::Mode>::Log(
    const param_type& p,
    std::string* l) {
  LogParam(static_cast<int>(p), l);
}

#if defined(OS_WIN)
void ParamTraits<PlatformFileForTransit>::Write(base::Pickle* m,
                                                const param_type& p) {
  m->WriteBool(p.IsValid());
  if (p.IsValid()) {
    HandleWin handle_win(p.GetHandle());
    ParamTraits<HandleWin>::Write(m, handle_win);
    ::CloseHandle(p.GetHandle());
  }
}

bool ParamTraits<PlatformFileForTransit>::Read(const base::Pickle* m,
                                               base::PickleIterator* iter,
                                               param_type* r) {
  bool is_valid;
  if (!iter->ReadBool(&is_valid))
    return false;
  if (!is_valid) {
    *r = PlatformFileForTransit();
    return true;
  }

  HandleWin handle_win;
  if (!ParamTraits<HandleWin>::Read(m, iter, &handle_win))
    return false;
  *r = PlatformFileForTransit(handle_win.get_handle());
  return true;
}

void ParamTraits<PlatformFileForTransit>::Log(const param_type& p,
                                              std::string* l) {
  LogParam(p.GetHandle(), l);
}
#endif  // defined(OS_WIN)

void ParamTraits<base::FilePath>::Write(base::Pickle* m, const param_type& p) {
  p.WriteToPickle(m);
}

bool ParamTraits<base::FilePath>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  return r->ReadFromPickle(iter);
}

void ParamTraits<base::FilePath>::Log(const param_type& p, std::string* l) {
  ParamTraits<base::FilePath::StringType>::Log(p.value(), l);
}

void ParamTraits<base::ListValue>::Write(base::Pickle* m, const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<base::ListValue>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) ||
      type != static_cast<int>(base::Value::Type::LIST))
    return false;

  return ReadListValue(m, iter, r, 0);
}

void ParamTraits<base::ListValue>::Log(const param_type& p, std::string* l) {
  std::string json;
  base::JSONWriter::Write(p, &json);
  l->append(json);
}

void ParamTraits<base::Value>::Write(base::Pickle* m, const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<base::Value>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    param_type* r) {
  return ReadValue(m, iter, r, 0);
}

void ParamTraits<base::Value>::Log(const param_type& p, std::string* l) {
  std::string json;
  base::JSONWriter::Write(p, &json);
  l->append(json);
}

void ParamTraits<base::NullableString16>::Write(base::Pickle* m,
                                                const param_type& p) {
  WriteParam(m, p.string());
  WriteParam(m, p.is_null());
}

bool ParamTraits<base::NullableString16>::Read(const base::Pickle* m,
                                               base::PickleIterator* iter,
                                               param_type* r) {
  base::string16 string;
  if (!ReadParam(m, iter, &string))
    return false;
  bool is_null;
  if (!ReadParam(m, iter, &is_null))
    return false;
  *r = base::NullableString16(string, is_null);
  return true;
}

void ParamTraits<base::NullableString16>::Log(const param_type& p,
                                              std::string* l) {
  l->append("(");
  LogParam(p.string(), l);
  l->append(", ");
  LogParam(p.is_null(), l);
  l->append(")");
}

void ParamTraits<base::File::Info>::Write(base::Pickle* m,
                                          const param_type& p) {
  WriteParam(m, p.size);
  WriteParam(m, p.is_directory);
  WriteParam(m, p.last_modified.ToDoubleT());
  WriteParam(m, p.last_accessed.ToDoubleT());
  WriteParam(m, p.creation_time.ToDoubleT());
}

bool ParamTraits<base::File::Info>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  double last_modified, last_accessed, creation_time;
  if (!ReadParam(m, iter, &p->size) ||
      !ReadParam(m, iter, &p->is_directory) ||
      !ReadParam(m, iter, &last_modified) ||
      !ReadParam(m, iter, &last_accessed) ||
      !ReadParam(m, iter, &creation_time))
    return false;
  p->last_modified = base::Time::FromDoubleT(last_modified);
  p->last_accessed = base::Time::FromDoubleT(last_accessed);
  p->creation_time = base::Time::FromDoubleT(creation_time);
  return true;
}

void ParamTraits<base::File::Info>::Log(const param_type& p,
                                        std::string* l) {
  l->append("(");
  LogParam(p.size, l);
  l->append(",");
  LogParam(p.is_directory, l);
  l->append(",");
  LogParam(p.last_modified.ToDoubleT(), l);
  l->append(",");
  LogParam(p.last_accessed.ToDoubleT(), l);
  l->append(",");
  LogParam(p.creation_time.ToDoubleT(), l);
  l->append(")");
}

void ParamTraits<base::Time>::Write(base::Pickle* m, const param_type& p) {
  ParamTraits<int64_t>::Write(m, p.ToInternalValue());
}

bool ParamTraits<base::Time>::Read(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   param_type* r) {
  int64_t value;
  if (!ParamTraits<int64_t>::Read(m, iter, &value))
    return false;
  *r = base::Time::FromInternalValue(value);
  return true;
}

void ParamTraits<base::Time>::Log(const param_type& p, std::string* l) {
  ParamTraits<int64_t>::Log(p.ToInternalValue(), l);
}

void ParamTraits<base::TimeDelta>::Write(base::Pickle* m, const param_type& p) {
  ParamTraits<int64_t>::Write(m, p.ToInternalValue());
}

bool ParamTraits<base::TimeDelta>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* r) {
  int64_t value;
  bool ret = ParamTraits<int64_t>::Read(m, iter, &value);
  if (ret)
    *r = base::TimeDelta::FromInternalValue(value);

  return ret;
}

void ParamTraits<base::TimeDelta>::Log(const param_type& p, std::string* l) {
  ParamTraits<int64_t>::Log(p.ToInternalValue(), l);
}

void ParamTraits<base::TimeTicks>::Write(base::Pickle* m, const param_type& p) {
  ParamTraits<int64_t>::Write(m, p.ToInternalValue());
}

bool ParamTraits<base::TimeTicks>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* r) {
  int64_t value;
  bool ret = ParamTraits<int64_t>::Read(m, iter, &value);
  if (ret)
    *r = base::TimeTicks::FromInternalValue(value);

  return ret;
}

void ParamTraits<base::TimeTicks>::Log(const param_type& p, std::string* l) {
  ParamTraits<int64_t>::Log(p.ToInternalValue(), l);
}

// If base::UnguessableToken is no longer 128 bits, the IPC serialization logic
// below should be updated.
static_assert(sizeof(base::UnguessableToken) == 2 * sizeof(uint64_t),
              "base::UnguessableToken should be of size 2 * sizeof(uint64_t).");

void ParamTraits<base::UnguessableToken>::Write(base::Pickle* m,
                                                const param_type& p) {
  DCHECK(!p.is_empty());

  ParamTraits<uint64_t>::Write(m, p.GetHighForSerialization());
  ParamTraits<uint64_t>::Write(m, p.GetLowForSerialization());
}

bool ParamTraits<base::UnguessableToken>::Read(const base::Pickle* m,
                                               base::PickleIterator* iter,
                                               param_type* r) {
  uint64_t high, low;
  if (!ParamTraits<uint64_t>::Read(m, iter, &high) ||
      !ParamTraits<uint64_t>::Read(m, iter, &low))
    return false;

  // Receiving a zeroed UnguessableToken is a security issue.
  if (high == 0 && low == 0)
    return false;

  *r = base::UnguessableToken::Deserialize(high, low);
  return true;
}

void ParamTraits<base::UnguessableToken>::Log(const param_type& p,
                                              std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<IPC::ChannelHandle>::Write(base::Pickle* m,
                                            const param_type& p) {
#if defined(OS_NACL_SFI)
  WriteParam(m, p.socket);
#else
  WriteParam(m, p.mojo_handle);
#endif
}

bool ParamTraits<IPC::ChannelHandle>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* r) {
#if defined(OS_NACL_SFI)
  return ReadParam(m, iter, &r->socket);
#else
  return ReadParam(m, iter, &r->mojo_handle);
#endif
}

void ParamTraits<IPC::ChannelHandle>::Log(const param_type& p,
                                          std::string* l) {
  l->append("ChannelHandle(");
#if defined(OS_NACL_SFI)
  ParamTraits<base::FileDescriptor>::Log(p.socket, l);
#else
  LogParam(p.mojo_handle, l);
#endif
  l->append(")");
}

void ParamTraits<LogData>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.channel);
  WriteParam(m, p.routing_id);
  WriteParam(m, p.type);
  WriteParam(m, p.flags);
  WriteParam(m, p.sent);
  WriteParam(m, p.receive);
  WriteParam(m, p.dispatch);
  WriteParam(m, p.message_name);
  WriteParam(m, p.params);
}

bool ParamTraits<LogData>::Read(const base::Pickle* m,
                                base::PickleIterator* iter,
                                param_type* r) {
  return
      ReadParam(m, iter, &r->channel) &&
      ReadParam(m, iter, &r->routing_id) &&
      ReadParam(m, iter, &r->type) &&
      ReadParam(m, iter, &r->flags) &&
      ReadParam(m, iter, &r->sent) &&
      ReadParam(m, iter, &r->receive) &&
      ReadParam(m, iter, &r->dispatch) &&
      ReadParam(m, iter, &r->message_name) &&
      ReadParam(m, iter, &r->params);
}

void ParamTraits<LogData>::Log(const param_type& p, std::string* l) {
  // Doesn't make sense to implement this!
}

void ParamTraits<Message>::Write(base::Pickle* m, const Message& p) {
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  // We don't serialize the file descriptors in the nested message, so there
  // better not be any.
  DCHECK(!p.HasAttachments());
#endif

  // Don't just write out the message. This is used to send messages between
  // NaCl (Posix environment) and the browser (could be on Windows). The message
  // header formats differ between these systems (so does handle sharing, but
  // we already asserted we don't have any handles). So just write out the
  // parts of the header we use.
  //
  // Be careful also to use only explicitly-sized types. The NaCl environment
  // could be 64-bit and the host browser could be 32-bits. The nested message
  // may or may not be safe to send between 32-bit and 64-bit systems, but we
  // leave that up to the code sending the message to ensure.
  m->WriteUInt32(static_cast<uint32_t>(p.routing_id()));
  m->WriteUInt32(p.type());
  m->WriteUInt32(p.flags());
  m->WriteData(p.payload(), static_cast<uint32_t>(p.payload_size()));
}

bool ParamTraits<Message>::Read(const base::Pickle* m,
                                base::PickleIterator* iter,
                                Message* r) {
  uint32_t routing_id, type, flags;
  if (!iter->ReadUInt32(&routing_id) ||
      !iter->ReadUInt32(&type) ||
      !iter->ReadUInt32(&flags))
    return false;

  int payload_size;
  const char* payload;
  if (!iter->ReadData(&payload, &payload_size))
    return false;

  r->SetHeaderValues(static_cast<int32_t>(routing_id), type, flags);
  r->WriteBytes(payload, payload_size);
  return true;
}

void ParamTraits<Message>::Log(const Message& p, std::string* l) {
  l->append("<IPC::Message>");
}

#if defined(OS_WIN)
// Note that HWNDs/HANDLE/HCURSOR/HACCEL etc are always 32 bits, even on 64
// bit systems. That's why we use the Windows macros to convert to 32 bits.
void ParamTraits<HANDLE>::Write(base::Pickle* m, const param_type& p) {
  m->WriteInt(HandleToLong(p));
}

bool ParamTraits<HANDLE>::Read(const base::Pickle* m,
                               base::PickleIterator* iter,
                               param_type* r) {
  int32_t temp;
  if (!iter->ReadInt(&temp))
    return false;
  *r = LongToHandle(temp);
  return true;
}

void ParamTraits<HANDLE>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("0x%p", p));
}

void ParamTraits<MSG>::Write(base::Pickle* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(&p), sizeof(MSG));
}

bool ParamTraits<MSG>::Read(const base::Pickle* m,
                            base::PickleIterator* iter,
                            param_type* r) {
  const char *data;
  int data_size = 0;
  bool result = iter->ReadData(&data, &data_size);
  if (result && data_size == sizeof(MSG)) {
    memcpy(r, data, sizeof(MSG));
  } else {
    result = false;
    NOTREACHED();
  }

  return result;
}

void ParamTraits<MSG>::Log(const param_type& p, std::string* l) {
  l->append("<MSG>");
}

#endif  // OS_WIN

}  // namespace IPC
