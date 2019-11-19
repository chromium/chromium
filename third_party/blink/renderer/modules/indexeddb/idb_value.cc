// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8.h"

namespace blink {

IDBValue::IDBValue(scoped_refptr<SharedBuffer> data,
                   Vector<WebBlobInfo> blob_info)
    : data_(std::move(data)), blob_info_(std::move(blob_info)) {}

IDBValue::~IDBValue() {
  if (isolate_ && external_allocated_size_)
    isolate_->AdjustAmountOfExternalAllocatedMemory(-external_allocated_size_);
}

scoped_refptr<SerializedScriptValue> IDBValue::CreateSerializedValue() const {
  return SerializedScriptValue::Create(data_);
}

bool IDBValue::IsNull() const {
  return !data_.get();
}

void IDBValue::SetIsolate(v8::Isolate* isolate) {
  DCHECK(isolate);
  DCHECK(!isolate_) << "SetIsolate must be called at most once";

  isolate_ = isolate;
  external_allocated_size_ = data_ ? static_cast<int64_t>(data_->size()) : 0l;
  if (external_allocated_size_)
    isolate_->AdjustAmountOfExternalAllocatedMemory(external_allocated_size_);
}

void IDBValue::SetData(scoped_refptr<SharedBuffer> new_data) {
  DCHECK(isolate_)
      << "Value unwrapping should be done after an isolate has been associated";
  DCHECK(new_data) << "Value unwrapping must result in a non-empty buffer";

  int64_t old_external_allocated_size = external_allocated_size_;
  external_allocated_size_ = new_data->size();
  isolate_->AdjustAmountOfExternalAllocatedMemory(external_allocated_size_ -
                                                  old_external_allocated_size);

  data_ = std::move(new_data);
}

scoped_refptr<BlobDataHandle> IDBValue::TakeLastBlob() {
  DCHECK_GT(blob_info_.size(), 0U)
      << "The IDBValue does not have any attached Blob";

  scoped_refptr<BlobDataHandle> return_value =
      blob_info_.back().GetBlobHandle();
  blob_info_.pop_back();

  return return_value;
}

}  // namespace blink
