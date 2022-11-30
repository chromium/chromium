// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_param_traits.h"

#include "base/logging.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "ipc/ipc_mojo_message_helper.h"

namespace IPC {

void ParamTraits<mojo::MessagePipeHandle>::Write(base::Pickle* m,
                                                 const param_type& p) {
  WriteParam(m, p.is_valid());
  if (p.is_valid())
    MojoMessageHelper::WriteMessagePipeTo(m, mojo::ScopedMessagePipeHandle(p));
}

bool ParamTraits<mojo::MessagePipeHandle>::Read(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                param_type* r) {
  bool is_valid;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;

  mojo::ScopedMessagePipeHandle handle;
  if (!MojoMessageHelper::ReadMessagePipeFrom(m, iter, &handle))
    return false;
  DCHECK(handle.is_valid());
  *r = handle.release();
  return true;
}

void ParamTraits<mojo::MessagePipeHandle>::Log(const param_type& p,
                                               std::string* l) {
  l->append("mojo::MessagePipeHandle(");
  LogParam(static_cast<uint64_t>(p.value()), l);
  l->append(")");
}

void ParamTraits<mojo::DataPipeConsumerHandle>::Write(base::Pickle* m,
                                                      const param_type& p) {
  WriteParam(m, p.is_valid());
  if (!p.is_valid())
    return;

  m->WriteAttachment(new internal::MojoHandleAttachment(
      mojo::ScopedHandle::From(mojo::ScopedDataPipeConsumerHandle(p))));
}

bool ParamTraits<mojo::DataPipeConsumerHandle>::Read(const base::Pickle* m,
                                                     base::PickleIterator* iter,
                                                     param_type* r) {
  bool is_valid;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;

  scoped_refptr<base::Pickle::Attachment> attachment;
  if (!m->ReadAttachment(iter, &attachment)) {
    DLOG(ERROR) << "Failed to read attachment for message pipe.";
    return false;
  }

  MessageAttachment::Type type =
      static_cast<MessageAttachment*>(attachment.get())->GetType();
  if (type != MessageAttachment::Type::MOJO_HANDLE) {
    DLOG(ERROR) << "Unexpected attachment type:" << static_cast<int>(type);
    return false;
  }

  mojo::ScopedDataPipeConsumerHandle handle;
  handle.reset(mojo::DataPipeConsumerHandle(
      static_cast<internal::MojoHandleAttachment*>(attachment.get())
          ->TakeHandle()
          .release()
          .value()));
  DCHECK(handle.is_valid());
  *r = handle.release();
  return true;
}

void ParamTraits<mojo::DataPipeConsumerHandle>::Log(const param_type& p,
                                                    std::string* l) {
  l->append("mojo::DataPipeConsumerHandle(");
  LogParam(static_cast<uint64_t>(p.value()), l);
  l->append(")");
}

}  // namespace IPC
