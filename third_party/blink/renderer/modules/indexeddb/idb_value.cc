// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8.h"

namespace blink {

IDBValue::IDBValue(
    Vector<char>&& data,
    Vector<WebBlobInfo> blob_info,
    Vector<mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>>
        file_system_access_tokens)
    : data_(std::move(data)),
      blob_info_(std::move(blob_info)),
      file_system_access_tokens_(std::move(file_system_access_tokens)) {}

IDBValue::~IDBValue() {
  if (isolate_) {
    external_memory_accounter_.Clear(isolate_.get());
  }
}

scoped_refptr<SerializedScriptValue> IDBValue::CreateSerializedValue() const {
  Vector<char> decompressed;
  if (IDBValueUnwrapper::Decompress(data_, &decompressed)) {
    const_cast<IDBValue*>(this)->SetData(std::move(decompressed));
  }
  return SerializedScriptValue::Create(base::as_byte_span(data_));
}

void IDBValue::SetIsolate(v8::Isolate* isolate) {
  DCHECK(isolate);
  DCHECK(!isolate_) << "SetIsolate must be called at most once";

  isolate_ = isolate;
  size_t external_allocated_size = DataSize();
  if (external_allocated_size) {
    external_memory_accounter_.Increase(isolate_.get(),
                                        external_allocated_size);
  }
}

void IDBValue::SetData(Vector<char>&& new_data) {
  DCHECK(isolate_)
      << "Value unwrapping should be done after an isolate has been associated";

  external_memory_accounter_.Set(isolate_.get(), new_data.size());

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

// static
std::unique_ptr<IDBValue> IDBValue::ConvertReturnValue(
    const mojom::blink::IDBReturnValuePtr& input) {
  if (!input) {
    return std::make_unique<IDBValue>(Vector<char>(), Vector<WebBlobInfo>());
  }

  std::unique_ptr<IDBValue> output = std::move(input->value);
  output->SetInjectedPrimaryKey(std::move(input->primary_key), input->key_path);
  return output;
}

}  // namespace blink
