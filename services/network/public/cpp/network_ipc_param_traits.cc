// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_ipc_param_traits.h"

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"

namespace IPC {

void ParamTraits<network::DataElement>::Write(base::Pickle* m,
                                              const param_type& p) {
  WriteParam(m, static_cast<int>(p.type()));
  switch (p.type()) {
    case network::mojom::DataElementType::kBytes: {
      m->WriteData(p.bytes(), static_cast<int>(p.length()));
      break;
    }
    case network::mojom::DataElementType::kFile: {
      WriteParam(m, p.path());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      WriteParam(m, p.expected_modification_time());
      break;
    }
    case network::mojom::DataElementType::kRawFile: {
      WriteParam(
          m, IPC::GetPlatformFileForTransit(p.file().GetPlatformFile(),
                                            false /* close_source_handle */));
      WriteParam(m, p.path());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      WriteParam(m, p.expected_modification_time());
      break;
    }
    case network::mojom::DataElementType::kBlob: {
      WriteParam(m, p.blob_uuid());
      WriteParam(m, p.offset());
      WriteParam(m, p.length());
      break;
    }
    case network::mojom::DataElementType::kDataPipe: {
      WriteParam(m, p.CloneDataPipeGetter().PassPipe().release());
      break;
    }
    case network::mojom::DataElementType::kChunkedDataPipe: {
      WriteParam(m, const_cast<network::DataElement&>(p)
                        .ReleaseChunkedDataPipeGetter()
                        .PassPipe()
                        .release());
      break;
    }
    case network::mojom::DataElementType::kUnknown: {
      NOTREACHED();
      break;
    }
  }
}

bool ParamTraits<network::DataElement>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;
  switch (static_cast<network::mojom::DataElementType>(type)) {
    case network::mojom::DataElementType::kBytes: {
      const char* data;
      int len;
      if (!iter->ReadData(&data, &len))
        return false;
      r->SetToBytes(data, len);
      return true;
    }
    case network::mojom::DataElementType::kFile: {
      base::FilePath file_path;
      uint64_t offset, length;
      base::Time expected_modification_time;
      if (!ReadParam(m, iter, &file_path))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      if (!ReadParam(m, iter, &expected_modification_time))
        return false;
      r->SetToFilePathRange(file_path, offset, length,
                            expected_modification_time);
      return true;
    }
    case network::mojom::DataElementType::kRawFile: {
      IPC::PlatformFileForTransit platform_file_for_transit;
      if (!ReadParam(m, iter, &platform_file_for_transit))
        return false;
      base::File file = PlatformFileForTransitToFile(platform_file_for_transit);
      base::FilePath file_path;
      if (!ReadParam(m, iter, &file_path))
        return false;
      uint64_t offset;
      if (!ReadParam(m, iter, &offset))
        return false;
      uint64_t length;
      if (!ReadParam(m, iter, &length))
        return false;
      base::Time expected_modification_time;
      if (!ReadParam(m, iter, &expected_modification_time))
        return false;
      r->SetToFileRange(std::move(file), file_path, offset, length,
                        expected_modification_time);
      return true;
    }
    case network::mojom::DataElementType::kBlob: {
      std::string blob_uuid;
      uint64_t offset, length;
      if (!ReadParam(m, iter, &blob_uuid))
        return false;
      if (!ReadParam(m, iter, &offset))
        return false;
      if (!ReadParam(m, iter, &length))
        return false;
      r->SetToBlobRange(blob_uuid, offset, length);
      return true;
    }
    case network::mojom::DataElementType::kDataPipe: {
      mojo::MessagePipeHandle message_pipe;
      if (!ReadParam(m, iter, &message_pipe))
        return false;
      mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter(
          mojo::ScopedMessagePipeHandle(message_pipe), 0u);
      r->SetToDataPipe(std::move(data_pipe_getter));
      return true;
    }
    case network::mojom::DataElementType::kChunkedDataPipe: {
      mojo::MessagePipeHandle message_pipe;
      if (!ReadParam(m, iter, &message_pipe))
        return false;
      mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
          chunked_data_pipe_getter(mojo::ScopedMessagePipeHandle(message_pipe),
                                   0u);

      r->SetToChunkedDataPipe(std::move(chunked_data_pipe_getter));
      return true;
    }
    case network::mojom::DataElementType::kUnknown: {
      NOTREACHED();
      return false;
    }
  }
  return false;
}

void ParamTraits<network::DataElement>::Log(const param_type& p,
                                            std::string* l) {
  l->append("<network::DataElement>");
}

void ParamTraits<scoped_refptr<network::ResourceRequestBody>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != nullptr);
  if (p.get()) {
    WriteParam(m, *p->elements());
    WriteParam(m, p->identifier());
    WriteParam(m, p->contains_sensitive_info());
  }
}

bool ParamTraits<scoped_refptr<network::ResourceRequestBody>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  std::vector<network::DataElement> elements;
  if (!ReadParam(m, iter, &elements))
    return false;
  // A chunked element is only allowed if it's the only one element.
  if (elements.size() > 1) {
    for (const auto& element : elements) {
      if (element.type() == network::mojom::DataElementType::kChunkedDataPipe)
        return false;
    }
  }
  int64_t identifier;
  if (!ReadParam(m, iter, &identifier))
    return false;
  bool contains_sensitive_info;
  if (!ReadParam(m, iter, &contains_sensitive_info))
    return false;
  *r = new network::ResourceRequestBody;
  (*r)->swap_elements(&elements);
  (*r)->set_identifier(identifier);
  (*r)->set_contains_sensitive_info(contains_sensitive_info);
  return true;
}

void ParamTraits<scoped_refptr<network::ResourceRequestBody>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<ResourceRequestBody>");
}

}  // namespace IPC

// Generation of IPC definitions.

// Generate constructors.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#include "ipc/struct_constructor_macros.h"
#include "network_ipc_param_traits.h"

// Generate param traits write methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "network_ipc_param_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "network_ipc_param_traits.h"
}  // namespace IPC

// Generate param traits log methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "network_ipc_param_traits.h"
}  // namespace IPC
