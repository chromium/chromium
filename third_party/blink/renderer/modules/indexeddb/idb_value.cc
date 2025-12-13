// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8.h"

namespace blink {

IDBValue::IDBValue() = default;

IDBValue::~IDBValue() {
  if (isolate_) {
    external_memory_accounter_.Clear(isolate_.get());
  }
}

scoped_refptr<SerializedScriptValue> IDBValue::CreateSerializedValue() const {
  SerializedScriptValue::DataBufferPtr decompressed;
  if (IDBValueUnwrapper::Decompress(Data(), /*out_buffer=*/nullptr,
                                    /*out_buffer_in_place=*/&decompressed)) {
    return SerializedScriptValue::Create(std::move(decompressed));
  }

  return SerializedScriptValue::Create(Data());
}

void IDBValue::SetIsolate(v8::Isolate* isolate) {
  DCHECK(isolate);
  DCHECK(!isolate_) << "SetIsolate must be called at most once";

  isolate_ = isolate;
  size_t external_allocated_size = Data().size();
  if (external_allocated_size) {
    external_memory_accounter_.Increase(isolate_.get(),
                                        external_allocated_size);
  }
}

base::span<const uint8_t> IDBValue::Data() const {
  if (data_from_mojo_.size() != 0) {
    return data_from_mojo_;
  }
  if (!massaged_data_.empty()) {
    return base::as_byte_span(massaged_data_);
  }
  return data_.as_span();
}

void IDBValue::SetBlobInfo(Vector<WebBlobInfo> blob_info) {
  blob_info_ = std::move(blob_info);
}

void IDBValue::SetData(Vector<char> massaged_data) {
  if (isolate_) {
    external_memory_accounter_.Set(isolate_.get(), massaged_data.size());
  }

  // This data (which has been [un]wrapped, [de]compressed, etc) may be
  // replacing existing data.
  massaged_data_ = std::move(massaged_data);
  data_from_mojo_ = mojo_base::BigBuffer();
  data_ = SerializedScriptValue::DataBufferPtr();
}

void IDBValue::SetData(SerializedScriptValue::DataBufferPtr new_data) {
  CHECK_EQ(data_from_mojo_.size(), 0U);
  CHECK(massaged_data_.empty());

  if (isolate_) {
    external_memory_accounter_.Set(isolate_.get(), new_data.size());
  }

  data_ = std::move(new_data);
}

void IDBValue::SetData(mojo_base::BigBuffer new_data) {
  // This overload is only used when unpacking a Mojo message from the browser.
  // The data may later be updated via unwrapping.
  CHECK(data_.empty());
  CHECK(massaged_data_.empty());

  if (isolate_) {
    external_memory_accounter_.Set(isolate_.get(), new_data.size());
  }

  data_from_mojo_ = std::move(new_data);
}

scoped_refptr<BlobDataHandle> IDBValue::TakeLastBlob() {
  DCHECK_GT(blob_info_.size(), 0U)
      << "The IDBValue does not have any attached Blob";

  scoped_refptr<BlobDataHandle> return_value =
      blob_info_.back().GetBlobHandle();
  blob_info_.pop_back();

  return return_value;
}

// static
std::unique_ptr<IDBValue> IDBValue::ConvertReturnValue(
    const mojom::blink::IDBReturnValuePtr& input) {
  if (!input) {
    return std::make_unique<IDBValue>();
  }

  std::unique_ptr<IDBValue> output = std::move(input->value);
  output->SetInjectedPrimaryKey(std::move(input->primary_key), input->key_path);
  return output;
}

}  // namespace blink
