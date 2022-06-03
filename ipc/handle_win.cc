// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_win.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_message.h"

namespace IPC {

HandleWin::HandleWin() : handle_(INVALID_HANDLE_VALUE) {}

HandleWin::HandleWin(const HANDLE& handle) : handle_(handle) {}

// static
void ParamTraits<HandleWin>::Write(base::Pickle* m, const param_type& p) {
  scoped_refptr<IPC::internal::HandleAttachmentWin> attachment(
      new IPC::internal::HandleAttachmentWin(p.get_handle()));
  if (!m->WriteAttachment(std::move(attachment)))
    NOTREACHED();
}

// static
bool ParamTraits<HandleWin>::Read(const base::Pickle* m,
                                  base::PickleIterator* iter,
                                  param_type* r) {
  scoped_refptr<base::Pickle::Attachment> base_attachment;
  if (!m->ReadAttachment(iter, &base_attachment))
    return false;
  MessageAttachment* attachment =
      static_cast<MessageAttachment*>(base_attachment.get());
  if (attachment->GetType() != MessageAttachment::Type::WIN_HANDLE)
    return false;
  IPC::internal::HandleAttachmentWin* handle_attachment =
      static_cast<IPC::internal::HandleAttachmentWin*>(attachment);
  r->set_handle(handle_attachment->Take());
  return true;
}

// static
void ParamTraits<HandleWin>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("0x%p", p.get_handle()));
}

}  // namespace IPC
