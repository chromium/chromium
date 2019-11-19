/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/clipboard/data_transfer_item_list.h"

#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

uint32_t DataTransferItemList::length() const {
  if (!data_transfer_->CanReadTypes())
    return 0;
  return data_object_->length();
}

DataTransferItem* DataTransferItemList::item(uint32_t index) {
  if (!data_transfer_->CanReadTypes())
    return nullptr;
  DataObjectItem* item = data_object_->Item(index);
  if (!item)
    return nullptr;

  return MakeGarbageCollected<DataTransferItem>(data_transfer_, item);
}

void DataTransferItemList::deleteItem(uint32_t index,
                                      ExceptionState& exception_state) {
  if (!data_transfer_->CanWriteData()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The list is not writable.");
    return;
  }
  data_object_->DeleteItem(index);
}

void DataTransferItemList::clear() {
  if (!data_transfer_->CanWriteData())
    return;
  data_object_->ClearAll();
}

DataTransferItem* DataTransferItemList::add(const String& data,
                                            const String& type,
                                            ExceptionState& exception_state) {
  if (!data_transfer_->CanWriteData())
    return nullptr;
  DataObjectItem* item = data_object_->Add(data, type);
  if (!item) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "An item already exists for type '" + type + "'.");
    return nullptr;
  }
  return MakeGarbageCollected<DataTransferItem>(data_transfer_, item);
}

DataTransferItem* DataTransferItemList::add(File* file) {
  if (!data_transfer_->CanWriteData())
    return nullptr;
  DataObjectItem* item = data_object_->Add(file);
  if (!item)
    return nullptr;
  return MakeGarbageCollected<DataTransferItem>(data_transfer_, item);
}

DataTransferItemList::DataTransferItemList(DataTransfer* data_transfer,
                                           DataObject* data_object)
    : data_transfer_(data_transfer), data_object_(data_object) {}

void DataTransferItemList::Trace(blink::Visitor* visitor) {
  visitor->Trace(data_transfer_);
  visitor->Trace(data_object_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
