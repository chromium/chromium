// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8.h"

namespace blink {

IDBValue::IDBValue(const WebData& data,
                   const WebVector<WebBlobInfo>& web_blob_info)
    : data_(data) {
  blob_info_.ReserveInitialCapacity(SafeCast<wtf_size_t>(web_blob_info.size()));

  for (const WebBlobInfo& info : web_blob_info) {
    blob_info_.push_back(info);
  }
}

IDBValue::IDBValue(scoped_refptr<SharedBuffer> unwrapped_data,
                   Vector<WebBlobInfo> blob_info)
    : data_(std::move(unwrapped_data)),
      blob_info_(std::move(blob_info)) {
}

IDBValue::~IDBValue() {
#if DCHECK_IS_ON()
  DCHECK_EQ(!!is_owned_by_web_idb_value_, !isolate_)
      << "IDBValues shold have associated isolates if and only if not owned by "
         "an WebIDBValue";
#endif  // DCHECK_IS_ON()

  if (isolate_ && external_allocated_size_)
    isolate_->AdjustAmountOfExternalAllocatedMemory(-external_allocated_size_);
}

std::unique_ptr<IDBValue> IDBValue::Create(
    const WebData& data,
    const WebVector<WebBlobInfo>& web_blob_info) {
  return base::WrapUnique(new IDBValue(data, web_blob_info));
}

std::unique_ptr<IDBValue> IDBValue::Create(
    scoped_refptr<SharedBuffer> unwrapped_data,
    Vector<WebBlobInfo> blob_info) {
  return base::WrapUnique(
      new IDBValue(std::move(unwrapped_data), std::move(blob_info)));
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

#if DCHECK_IS_ON()
  DCHECK(!is_owned_by_web_idb_value_)
      << "IDBValues owned by an WebIDBValue cannot have associated isolates";
#endif  // DCHECK_IS_ON()

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

#if DCHECK_IS_ON()

void IDBValue::SetIsOwnedByWebIDBValue(bool is_owned_by_web_idb_value) {
  DCHECK(!isolate_ || !is_owned_by_web_idb_value)
      << "IDBValues owned by an WebIDBValue cannot have associated isolates";
  is_owned_by_web_idb_value_ = is_owned_by_web_idb_value;
}

#endif  // DCHECK_IS_ON()

}  // namespace blink
