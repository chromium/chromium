// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_VALUE_WRAPPING_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_VALUE_WRAPPING_H_

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class Blob;
class BlobDataHandle;
class ExceptionState;
class IDBValue;
class ScriptState;
class ScriptValue;
class SerializedScriptValue;

const base::Feature kIndexedDBLargeValueWrapping{
    "IndexedDBLargeValueWrapping", base::FEATURE_DISABLED_BY_DEFAULT};

// Logic for serializing V8 values for storage in IndexedDB.
//
// An IDBValueWrapper instance drives the serialization of a single V8 value to
// IndexedDB. An instance's lifecycle goes through the following stages:
// 1) Cloning - Right after an instance is constructed, its internal
//    representation is optimized for structured cloning via the Clone() method.
//    This may be necessary when extracting the primary key and/or index keys
//    for the serialized value.
// 2) Wrapping - DoneCloning() transitions the instance to an internal
//    representation optimized for wrapping via WrapIfBiggerThan().
// 3) Reading results - After any desired wrapping is performed, the Take*()
//    methods yield the serialized value components passed to the backing store.
//    To avoid unnecessary copies, the Take*() methods move out parts of the
//    internal representation, so each Take*() method can be called at most
//    once.
//
// Example usage:
//     auto wrapper = new IDBValueWrapper();
//     wrapper.Clone(...);  // Structured clone used to extract keys.
//     wrapper.DoneCloning();
//     wrapper.WrapIfBiggerThan(kWrapThreshold);
//     wrapper.TakeWireBytes();
//     wrapper.TakeBlobDataHandles();
//     wrapper.TakeBlobInfo();
//
// V8 values are stored on disk using the format implemented in
// SerializedScriptValue (SSV), which is essentialy a byte array plus an array
// of attached Blobs. For "normal" (not too large) V8 values, the SSV output's
// byte array is stored directly in IndexedDB's backing store, together with
// references to the attached Blobs.
//
// "Large" V8 values are wrapped in Blobs, in order to avoid operating the
// backing store in a sub-optimal region. Specifically, the byte array in the
// SSV output is replaced with a "wrapped value" marker, and stored inside a
// Blob that is tacked to the end of the SSV's Blob array. IndexedDB's backing
// store receives the "wrapped value" marker and the references to the Blobs,
// while the large byte array in the SSV output is handled by the Blob storage
// system.
//
// In summary:
// "normal" v8::Value -> SSV -> IDBValue (stores SSV output) -> LevelDB
// "large" v8::Value -> SSV -> IDBValue (stores SSV output) ->
//     Blob (stores SSV output) + IDBValue (stores Blob reference) -> LevelDB
//
// Full picture that accounts for Blob attachments:
// "normal" v8::Value -> SSV (byte array, Blob attachments) ->
//     IDBValue (bytes: SSV byte array, blobs: SSV Blob attachments) -> LevelDB
// "large" v8::Value -> SSV (byte array, Blob attachments) ->
//     IDBValue (bytes: "wrapped value" marker,
//               blobs: SSV Blob attachments + [wrapper Blob(SSV byte array)] ->
//     LevelDB
class MODULES_EXPORT IDBValueWrapper {
  STACK_ALLOCATED();

 public:
  // Wrapper for an IndexedDB value.
  //
  // The serialization process can throw an exception. The caller is responsible
  // for checking exception_state.
  //
  // The wrapper's internal representation is optimized for cloning the
  // serialized value. DoneCloning() must be called to transition to an internal
  // representation optimized for writing.
  IDBValueWrapper(
      v8::Isolate*,
      v8::Local<v8::Value>,
      SerializedScriptValue::SerializeOptions::WasmSerializationPolicy,
      ExceptionState&);
  ~IDBValueWrapper();

  // Creates a clone of the serialized value.
  //
  // This method is used to fulfill the IndexedDB specification requirement that
  // a value's key and index keys are extracted from a structured clone of the
  // value, which avoids the issue of side-effects in custom getters.
  //
  // This method cannot be called after DoneCloning().
  void Clone(ScriptState*, ScriptValue* clone);

  // Optimizes the serialized value's internal representation for writing to
  // disk.
  //
  // This must be called before Take*() methods can be called. After this method
  // is called, Clone() cannot be called anymore.
  void DoneCloning();

  // Conditionally wraps the serialized value's byte array into a Blob.
  //
  // The byte array is wrapped if its size exceeds max_bytes. In production, the
  // max_bytes threshold is currently always kWrapThreshold.
  //
  // This method must be called before the Take*() methods are called.
  bool WrapIfBiggerThan(unsigned max_bytes);

  // Obtains the byte array for the serialized value.
  //
  // This method must be called at most once, and must be called after
  // WrapIfBiggerThan().
  scoped_refptr<SharedBuffer> TakeWireBytes();

  // Obtains the BlobDataHandles from the serialized value's Blob array.
  //
  // This method must be called at most once, and must be called after
  // DoneCloning().
  Vector<scoped_refptr<BlobDataHandle>> TakeBlobDataHandles() {
#if DCHECK_IS_ON()
    DCHECK(done_cloning_) << __func__ << " called before DoneCloning()";
    DCHECK(owns_blob_handles_) << __func__ << " called twice";
    owns_blob_handles_ = false;
#endif  // DCHECK_IS_ON()

    return std::move(blob_handles_);
  }

  // Obtains WebBlobInfos for the serialized value's Blob array.
  //
  // This method must be called at most once, and must be called after
  // DoneCloning().
  inline Vector<WebBlobInfo> TakeBlobInfo() {
#if DCHECK_IS_ON()
    DCHECK(done_cloning_) << __func__ << " called before DoneCloning()";
    DCHECK(owns_blob_info_) << __func__ << " called twice";
    owns_blob_info_ = false;
#endif  // DCHECK_IS_ON()
    return std::move(blob_info_);
  }

  size_t DataLengthBeforeWrapInBytes() { return original_data_length_; }

  // Default threshold for WrapIfBiggerThan().
  //
  // This should be tuned to achieve a compromise between short-term IndexedDB
  // throughput and long-term I/O load and memory usage. LevelDB, the underlying
  // storage for IndexedDB, was not designed with large values in mind. At the
  // very least, large values will slow down compaction, causing occasional I/O
  // spikes.
  static constexpr unsigned kWrapThreshold = 64 * 1024;

  // MIME type used for Blobs that wrap IDBValues.
  static constexpr const char* kWrapMimeType =
      "application/vnd.blink-idb-value-wrapper";

  // Used to serialize the wrapped value. Exposed for testing.
  static void WriteVarInt(unsigned value, Vector<char>& output);
  static void WriteBytes(const Vector<uint8_t>& bytes, Vector<char>& output);

 private:
  // V8 value serialization state.
  scoped_refptr<SerializedScriptValue> serialized_value_;
  Vector<scoped_refptr<BlobDataHandle>> blob_handles_;
  Vector<WebBlobInfo> blob_info_;

  // Buffer for wire data that is not stored in SerializedScriptValue.
  //
  // This buffer ends up storing metadata generated by wrapping operations.
  Vector<char> wire_data_buffer_;

  // Points into SerializedScriptValue's data buffer, or into wire_data_buffer_.
  base::span<const uint8_t> wire_data_;

  size_t original_data_length_ = 0;

#if DCHECK_IS_ON()
  // Accounting for lifecycle stages.
  bool had_exception_ = false;
  bool done_cloning_ = false;
  bool owns_blob_handles_ = true;
  bool owns_blob_info_ = true;
  bool owns_wire_bytes_ = true;
#endif  // DCHECK_IS_ON()
};

// State and logic for unwrapping large IndexedDB values from Blobs.
//
// See IDBValueWrapper for an explanation of the wrapping concept.
//
// Once created, an IDBValueUnwrapper instance can be used to unwrap multiple
// Blobs. For each Blob to be unwrapped, the caller should first call Parse().
// If the method succeeds, the IDBValueUnwrapper will store the parse state,
// which can be obtained using WrapperBlobSize() and WrapperBlobHandle().
class MODULES_EXPORT IDBValueUnwrapper {
  STACK_ALLOCATED();

 public:
  IDBValueUnwrapper();

  // True if the IDBValue's data was wrapped in a Blob.
  static bool IsWrapped(IDBValue*);

  // True if at least one of the IDBValues' data was wrapped in a Blob.
  static bool IsWrapped(const Vector<std::unique_ptr<IDBValue>>&);

  // Unwraps an IDBValue that has wrapped Blob data.
  //
  // The caller should own the IDBValue (have a std::unique_ptr for it).
  static void Unwrap(scoped_refptr<SharedBuffer>&& wrapper_blob_content,
                     IDBValue* wrapped_value);

  // Parses the wrapper Blob information from a wrapped IDBValue.
  //
  // Returns true for success, and false for failure. Failure can mean that the
  // given value was not a wrapped IDBValue, or that the value bytes were
  // corrupted.
  bool Parse(IDBValue*);

  // Returns the size of the Blob obtained by the last Unwrap() call.
  //
  // Should only be called after a successful result from Unwrap().
  inline unsigned WrapperBlobSize() const {
    DCHECK(end_);
    return blob_size_;
  }

  // Returns a handle to the Blob obtained by the last Unwrap() call.
  //
  // Should only be called exactly once after a successful result from Unwrap().
  scoped_refptr<BlobDataHandle> WrapperBlobHandle();

 private:
  // Only present in tests.
  friend class IDBValueUnwrapperReadTestHelper;

  // Used to deserialize the wrapped value.
  bool ReadVarInt(unsigned&);
  bool ReadBytes(Vector<uint8_t>&);

  // Resets the parsing state.
  bool Reset();

  // Deserialization cursor in the SharedBuffer of the IDBValue being unwrapped.
  const uint8_t* current_;

  // Smallest invalid position_ value.
  const uint8_t* end_;

  // The size of the Blob holding the data for the last unwrapped IDBValue.
  unsigned blob_size_;

  // Handle to the Blob holding the data for the last unwrapped IDBValue.
  scoped_refptr<BlobDataHandle> blob_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_VALUE_WRAPPING_H_
