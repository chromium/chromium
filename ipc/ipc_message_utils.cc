// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ipc/ipc_message_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <type_traits>

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

#if BUILDFLAG(IS_APPLE)
#include "ipc/mach_port_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <tchar.h>

#include "ipc/handle_win.h"
#include "ipc/ipc_platform_file.h"
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/fuchsia_logging.h"
#include "ipc/handle_attachment_fuchsia.h"
#endif

#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_WIN)
  // Windows has a GUI for logging, which can handle arbitrary binary data.
  for (size_t i = 0; i < data.size(); ++i)
    out->push_back(data[i]);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // On POSIX, we log to stdout, which we assume can display ASCII.
  static const size_t kMaxBytesToLog = 100;
  for (size_t i = 0; i < std::min(data.size(), kMaxBytesToLog); ++i) {
    if (absl::ascii_isprint(static_cast<unsigned char>(data[i]))) {
      out->push_back(data[i]);
    } else {
      out->append(
          base::StringPrintf("[%02X]", static_cast<unsigned char>(data[i])));
    }
  }
  if (data.size() > kMaxBytesToLog) {
    out->append(base::StringPrintf(
        " and %u more bytes",
        static_cast<unsigned>(data.size() - kMaxBytesToLog)));
  }
#endif
}

template <typename CharType>
void WriteCharVector(base::Pickle* m, const std::vector<CharType>& p) {
  static_assert(sizeof(CharType) == 1);
  static_assert(std::is_integral_v<CharType>);
  if (p.empty()) {
    m->WriteData(nullptr, 0);
  } else {
    const char* data = reinterpret_cast<const char*>(p.data());
    m->WriteData(data, p.size());
  }
}

template <typename CharType>
bool ReadCharVector(const base::Pickle* m,
                    base::PickleIterator* iter,
                    std::vector<CharType>* r) {
  static_assert(sizeof(CharType) == 1);
  static_assert(std::is_integral_v<CharType>);
  const char* data;
  size_t data_size = 0;
  if (!iter->ReadData(&data, &data_size)) {
    return false;
  }
  const CharType* begin = reinterpret_cast<const CharType*>(data);
  const CharType* end = begin + data_size;
  r->assign(begin, end);
  return true;
}

void WriteValue(const base::Value& value, int recursion, base::Pickle* pickle);

void WriteDictValue(const base::Value::Dict& value,
                    int recursion,
                    base::Pickle* pickle) {
  WriteParam(pickle, base::checked_cast<int>(value.size()));
  for (const auto entry : value) {
    WriteParam(pickle, entry.first);
    WriteValue(entry.second, recursion + 1, pickle);
  }
}

void WriteListValue(const base::Value::List& value,
                    int recursion,
                    base::Pickle* pickle) {
  WriteParam(pickle, base::checked_cast<int>(value.size()));
  for (const auto& entry : value) {
    WriteValue(entry, recursion + 1, pickle);
  }
}

void WriteValue(const base::Value& value, int recursion, base::Pickle* pickle) {
  bool result;
  if (recursion > kMaxRecursionDepth) {
    LOG(ERROR) << "Max recursion depth hit in WriteValue.";
    return;
  }

  pickle->WriteInt(static_cast<int>(value.type()));

  switch (value.type()) {
    case base::Value::Type::NONE:
      break;
    case base::Value::Type::BOOLEAN: {
      WriteParam(pickle, value.GetBool());
      break;
    }
    case base::Value::Type::INTEGER: {
      DCHECK(value.is_int());
      WriteParam(pickle, value.GetInt());
      break;
    }
    case base::Value::Type::DOUBLE: {
      DCHECK(value.is_int() || value.is_double());
      WriteParam(pickle, value.GetDouble());
      break;
    }
    case base::Value::Type::STRING: {
      const std::string* val = value.GetIfString();
      result = !!val;
      DCHECK(result);
      WriteParam(pickle, *val);
      break;
    }
    case base::Value::Type::BINARY: {
      pickle->WriteData(reinterpret_cast<const char*>(value.GetBlob().data()),
                        value.GetBlob().size());
      break;
    }
    case base::Value::Type::DICT: {
      DCHECK(value.is_dict());
      WriteDictValue(value.GetDict(), recursion, pickle);
      break;
    }
    case base::Value::Type::LIST: {
      DCHECK(value.is_list());
      WriteListValue(value.GetList(), recursion, pickle);
      break;
    }
  }
}

bool ReadValue(const base::Pickle* pickle,
               base::PickleIterator* iter,
               int recursion,
               base::Value* value);

// Helper for ReadValue that reads a Value::Dict into a pre-allocated object.
bool ReadDictValue(const base::Pickle* pickle,
                   base::PickleIterator* iter,
                   int recursion,
                   base::Value::Dict* value) {
  int size;
  if (!ReadParam(pickle, iter, &size))
    return false;

  for (int i = 0; i < size; i++) {
    std::string key;
    base::Value subvalue;
    if (!ReadParam(pickle, iter, &key) ||
        !ReadValue(pickle, iter, recursion + 1, &subvalue)) {
      return false;
    }
    value->Set(key, std::move(subvalue));
  }

  return true;
}

// Helper for ReadValue that reads a Value::List into a pre-allocated object.
bool ReadListValue(const base::Pickle* pickle,
                   base::PickleIterator* iter,
                   int recursion,
                   base::Value::List* value) {
  int size;
  if (!ReadParam(pickle, iter, &size))
    return false;

  value->reserve(size);
  for (int i = 0; i < size; i++) {
    base::Value subval;
    if (!ReadValue(pickle, iter, recursion + 1, &subval))
      return false;
    value->Append(std::move(subval));
  }
  return true;
}

bool ReadValue(const base::Pickle* pickle,
               base::PickleIterator* iter,
               int recursion,
               base::Value* value) {
  if (recursion > kMaxRecursionDepth) {
    LOG(ERROR) << "Max recursion depth hit in ReadValue.";
    return false;
  }

  int type;
  if (!ReadParam(pickle, iter, &type))
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
      if (!ReadParam(pickle, iter, &val))
        return false;
      *value = base::Value(val);
      break;
    }
    case base::Value::Type::INTEGER: {
      int val;
      if (!ReadParam(pickle, iter, &val))
        return false;
      *value = base::Value(val);
      break;
    }
    case base::Value::Type::DOUBLE: {
      double val;
      if (!ReadParam(pickle, iter, &val))
        return false;
      *value = base::Value(val);
      break;
    }
    case base::Value::Type::STRING: {
      std::string val;
      if (!ReadParam(pickle, iter, &val))
        return false;
      *value = base::Value(std::move(val));
      break;
    }
    case base::Value::Type::BINARY: {
      std::optional<base::span<const uint8_t>> data = iter->ReadData();
      if (!data) {
        return false;
      }
      *value = base::Value(*data);
      break;
    }
    case base::Value::Type::DICT: {
      base::Value::Dict val;
      if (!ReadDictValue(pickle, iter, recursion, &val))
        return false;
      *value = base::Value(std::move(val));
      break;
    }
    case base::Value::Type::LIST: {
      base::Value::List val;
      if (!ReadListValue(pickle, iter, recursion, &val))
        return false;
      *value = base::Value(std::move(val));
      break;
    }
    default:
      NOTREACHED();
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) ||                                              \
    (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_64_BITS))
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

void ParamTraits<std::u16string>::Log(const param_type& p, std::string* l) {
  l->append(base::UTF16ToUTF8(p));
}

#if BUILDFLAG(IS_WIN)
bool ParamTraits<std::wstring>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* r) {
  std::u16string_view piece16;
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
  WriteCharVector(m, p);
}

bool ParamTraits<std::vector<char>>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  return ReadCharVector(m, iter, r);
}

void ParamTraits<std::vector<char>>::Log(const param_type& p, std::string* l) {
  LogBytes(p, l);
}

void ParamTraits<std::vector<unsigned char>>::Write(base::Pickle* m,
                                                    const param_type& p) {
  WriteCharVector(m, p);
}

bool ParamTraits<std::vector<unsigned char>>::Read(const base::Pickle* m,
                                                   base::PickleIterator* iter,
                                                   param_type* r) {
  return ReadCharVector(m, iter, r);
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
  size_t size;
  if (!iter->ReadLength(&size))
    return false;
  r->resize(size);
  for (size_t i = 0; i < size; i++) {
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

void ParamTraits<base::Value::Dict>::Write(base::Pickle* m,
                                           const param_type& p) {
  WriteDictValue(p, 0, m);
}

bool ParamTraits<base::Value::Dict>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  return ReadDictValue(m, iter, 0, r);
}

void ParamTraits<base::Value::Dict>::Log(const param_type& p, std::string* l) {
  std::string json;
  base::JSONWriter::Write(p, &json);
  l->append(json);
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
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
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
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
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_WIN)
  base::win::ScopedHandle h = const_cast<param_type&>(p).PassPlatformHandle();
  HandleWin handle_win(h.Get());
  WriteParam(m, handle_win);
#elif BUILDFLAG(IS_FUCHSIA)
  zx::vmo vmo = const_cast<param_type&>(p).PassPlatformHandle();
  WriteParam(m, vmo);
#elif BUILDFLAG(IS_APPLE)
  base::apple::ScopedMachSendRight h =
      const_cast<param_type&>(p).PassPlatformHandle();
  MachPortMac mach_port_mac(h.get());
  WriteParam(m, mach_port_mac);
#elif BUILDFLAG(IS_ANDROID)
  m->WriteAttachment(new internal::PlatformFileAttachment(
      base::ScopedFD(const_cast<param_type&>(p).PassPlatformHandle())));
#elif BUILDFLAG(IS_POSIX)
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

#if BUILDFLAG(IS_WIN)
  HandleWin handle_win;
  if (!ReadParam(m, iter, &handle_win))
    return false;
  *r = base::subtle::PlatformSharedMemoryRegion::Take(
      base::win::ScopedHandle(handle_win.get_handle()), mode, size, guid);
#elif BUILDFLAG(IS_FUCHSIA)
  zx::vmo vmo;
  if (!ReadParam(m, iter, &vmo))
    return false;
  *r = base::subtle::PlatformSharedMemoryRegion::Take(std::move(vmo), mode,
                                                      size, guid);
#elif BUILDFLAG(IS_APPLE)
  MachPortMac mach_port_mac;
  if (!ReadParam(m, iter, &mach_port_mac))
    return false;
  *r = base::subtle::PlatformSharedMemoryRegion::Take(
      base::apple::ScopedMachSendRight(mach_port_mac.get_mach_port()), mode,
      size, guid);
#elif BUILDFLAG(IS_POSIX)
  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!m->ReadAttachment(iter, &attachment))
    return false;
  if (static_cast<MessageAttachment*>(attachment.get())->GetType() !=
      MessageAttachment::Type::PLATFORM_FILE) {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

#endif

  return true;
}

void ParamTraits<base::subtle::PlatformSharedMemoryRegion>::Log(
    const param_type& p,
    std::string* l) {
#if BUILDFLAG(IS_FUCHSIA)
  l->append("Handle: ");
  LogParam(p.GetPlatformHandle()->get(), l);
#elif BUILDFLAG(IS_WIN)
  l->append("Handle: ");
  LogParam(p.GetPlatformHandle(), l);
#elif BUILDFLAG(IS_APPLE)
  l->append("Mach port: ");
  LogParam(p.GetPlatformHandle(), l);
#elif BUILDFLAG(IS_ANDROID)
  l->append("FD: ");
  LogParam(p.GetPlatformHandle(), l);
#elif BUILDFLAG(IS_POSIX)
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

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

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

void ParamTraits<base::Value::List>::Write(base::Pickle* m,
                                           const param_type& p) {
  WriteListValue(p, 0, m);
}

bool ParamTraits<base::Value::List>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  return ReadListValue(m, iter, 0, r);
}

void ParamTraits<base::Value::List>::Log(const param_type& p, std::string* l) {
  std::string json;
  base::JSONWriter::Write(p, &json);
  l->append(json);
}

void ParamTraits<base::Value>::Write(base::Pickle* m, const param_type& p) {
  WriteValue(p, 0, m);
}

bool ParamTraits<base::Value>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    param_type* r) {
  return ReadValue(m, iter, 0, r);
}

void ParamTraits<base::Value>::Log(const param_type& p, std::string* l) {
  std::string json;
  base::JSONWriter::Write(p, &json);
  l->append(json);
}

void ParamTraits<base::File::Info>::Write(base::Pickle* m,
                                          const param_type& p) {
  WriteParam(m, p.size);
  WriteParam(m, p.is_directory);
  WriteParam(m, p.last_modified.InSecondsFSinceUnixEpoch());
  WriteParam(m, p.last_accessed.InSecondsFSinceUnixEpoch());
  WriteParam(m, p.creation_time.InSecondsFSinceUnixEpoch());
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
  p->last_modified = base::Time::FromSecondsSinceUnixEpoch(last_modified);
  p->last_accessed = base::Time::FromSecondsSinceUnixEpoch(last_accessed);
  p->creation_time = base::Time::FromSecondsSinceUnixEpoch(creation_time);
  return true;
}

void ParamTraits<base::File::Info>::Log(const param_type& p,
                                        std::string* l) {
  l->append("(");
  LogParam(p.size, l);
  l->append(",");
  LogParam(p.is_directory, l);
  l->append(",");
  LogParam(p.last_modified.InSecondsFSinceUnixEpoch(), l);
  l->append(",");
  LogParam(p.last_accessed.InSecondsFSinceUnixEpoch(), l);
  l->append(",");
  LogParam(p.creation_time.InSecondsFSinceUnixEpoch(), l);
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

  // This is not mapped as nullable_is_same_type, so any UnguessableToken
  // deserialized by the traits should always yield a non-empty token.
  // If deserialization results in an empty token, the data is malformed.
  std::optional<base::UnguessableToken> token =
      base::UnguessableToken::Deserialize(high, low);
  if (!token.has_value()) {
    return false;
  }

  *r = token.value();
  return true;
}

void ParamTraits<base::UnguessableToken>::Log(const param_type& p,
                                              std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<IPC::ChannelHandle>::Write(base::Pickle* m,
                                            const param_type& p) {
#if BUILDFLAG(IS_NACL)
  WriteParam(m, p.socket);
#else
  WriteParam(m, p.mojo_handle);
#endif
}

bool ParamTraits<IPC::ChannelHandle>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* r) {
#if BUILDFLAG(IS_NACL)
  return ReadParam(m, iter, &r->socket);
#else
  return ReadParam(m, iter, &r->mojo_handle);
#endif
}

void ParamTraits<IPC::ChannelHandle>::Log(const param_type& p,
                                          std::string* l) {
  l->append("ChannelHandle(");
#if BUILDFLAG(IS_NACL)
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
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
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
  m->WriteData(p.payload_bytes());
}

bool ParamTraits<Message>::Read(const base::Pickle* m,
                                base::PickleIterator* iter,
                                Message* r) {
  uint32_t routing_id, type, flags;
  if (!iter->ReadUInt32(&routing_id) ||
      !iter->ReadUInt32(&type) ||
      !iter->ReadUInt32(&flags))
    return false;

  size_t payload_size;
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

#if BUILDFLAG(IS_WIN)
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
  size_t data_size = 0;
  bool result = iter->ReadData(&data, &data_size);
  if (result && data_size == sizeof(MSG)) {
    memcpy(r, data, sizeof(MSG));
  } else {
    NOTREACHED();
  }

  return result;
}

void ParamTraits<MSG>::Log(const param_type& p, std::string* l) {
  l->append("<MSG>");
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace IPC
