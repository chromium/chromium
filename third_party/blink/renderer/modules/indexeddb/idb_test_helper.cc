// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_test_helper.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

std::unique_ptr<IDBValue> CreateNullIDBValueForTesting(v8::Isolate* isolate) {
  scoped_refptr<SerializedScriptValue> null_ssv =
      SerializedScriptValue::NullValue();

  base::span<const uint8_t> ssv_wire_bytes = null_ssv->GetWireData();

  auto idb_value = std::make_unique<IDBValue>(Vector<char>(ssv_wire_bytes),
                                              Vector<WebBlobInfo>());
  idb_value->SetInjectedPrimaryKey(IDBKey::CreateNumber(42.0),
                                   IDBKeyPath(String("primaryKey")));
  return idb_value;
}

std::unique_ptr<IDBValue> CreateIDBValueForTesting(v8::Isolate* isolate,
                                                   bool create_wrapped_value) {
  uint32_t element_count = create_wrapped_value ? 16 : 2;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> v8_array = v8::Array::New(isolate, element_count);
  for (uint32_t i = 0; i < element_count; ++i)
    v8_array->Set(context, i, v8::True(isolate)).Check();

  NonThrowableExceptionState non_throwable_exception_state;
  IDBValueWrapper wrapper(isolate, v8_array,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state);
  wrapper.set_wrapping_threshold_for_test(
      create_wrapped_value ? 0 : 1024 * element_count);
  wrapper.DoneCloning();

  Vector<scoped_refptr<BlobDataHandle>> blob_data_handles =
      wrapper.TakeBlobDataHandles();
  Vector<WebBlobInfo> blob_infos = wrapper.TakeBlobInfo();

  auto idb_value = std::make_unique<IDBValue>(wrapper.TakeWireBytes(),
                                              std::move(blob_infos));
  idb_value->SetInjectedPrimaryKey(IDBKey::CreateNumber(42.0),
                                   IDBKeyPath(String("primaryKey")));

  DCHECK_EQ(create_wrapped_value,
            IDBValueUnwrapper::IsWrapped(idb_value.get()));
  return idb_value;
}

}  // namespace blink
