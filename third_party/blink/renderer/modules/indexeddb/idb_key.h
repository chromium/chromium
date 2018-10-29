/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_KEY_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// An IndexedDB primary or index key.
//
// The IndexedDB backing store regards script values written as object store
// record values as fairly opaque data (see IDBValue and IDBValueWrapping).
// However, it needs a fair amount of visibility into script values used as
// primary keys and index keys. For this reason, keys are represented using a
// dedicated data type that fully exposes its contents to the backing store.
class MODULES_EXPORT IDBKey {
 public:
  typedef Vector<std::unique_ptr<IDBKey>> KeyArray;

  static std::unique_ptr<IDBKey> CreateInvalid() {
    return base::WrapUnique(new IDBKey());
  }

  static std::unique_ptr<IDBKey> CreateNumber(double number) {
    return base::WrapUnique(new IDBKey(kNumberType, number));
  }

  static std::unique_ptr<IDBKey> CreateBinary(
      scoped_refptr<SharedBuffer> binary) {
    return base::WrapUnique(new IDBKey(std::move(binary)));
  }

  static std::unique_ptr<IDBKey> CreateString(const String& string) {
    return base::WrapUnique(new IDBKey(string));
  }

  static std::unique_ptr<IDBKey> CreateDate(double date) {
    return base::WrapUnique(new IDBKey(kDateType, date));
  }

  static std::unique_ptr<IDBKey> CreateArray(KeyArray array) {
    return base::WrapUnique(new IDBKey(std::move(array)));
  }

  // TODO(cmp): This |Clone| function is necessary for WebIDBKey's ctor
  //            functions.  It needs to be available in this header file so
  //            web_idb_key.cc can use it.  When the IDB Blink variant typemap
  //            moves to the renderer/modules/indexeddb/ types and off of the
  //            WebIDB* types, this |Clone| function should be removed.
  static std::unique_ptr<IDBKey> Clone(const std::unique_ptr<IDBKey>& rkey_in) {
    IDBKey* rkey = rkey_in.get();
    if (!rkey_in.get())
      return nullptr;

    switch (rkey->GetType()) {
      case kInvalidType:
        return IDBKey::CreateInvalid();
      case kArrayType: {
        IDBKey::KeyArray lkey_array;
        const auto& rkey_array = rkey->Array();
        for (const auto& rkey_item : rkey_array)
          lkey_array.push_back(IDBKey::Clone(rkey_item));
        return IDBKey::CreateArray(std::move(lkey_array));
      }
      case kBinaryType:
        return IDBKey::CreateBinary(rkey->Binary());
      case kStringType:
        return IDBKey::CreateString(rkey->GetString());
      case kDateType:
        return IDBKey::CreateDate(rkey->Date());
      case kNumberType:
        return IDBKey::CreateNumber(rkey->Number());

      case kTypeEnumMax:
        break;  // Not used, NOTREACHED.
    }
    NOTREACHED();
    return nullptr;
  }

  ~IDBKey();

  // Very rough estimate of minimum key size overhead.
  //
  // TODO(cmp): When the reference to this in web_idb_key.cc goes away, move
  //            this variable back to idb_key.cc's anonymous namespace.
  static const size_t kIDBKeyOverheadSize = 16;

  // In order of the least to the highest precedent in terms of sort order.
  // These values are written to logs. New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum Type {
    kInvalidType = 0,
    kArrayType = 1,
    kBinaryType = 2,
    kStringType = 3,
    kDateType = 4,
    kNumberType = 5,
    kTypeEnumMax,
  };

  Type GetType() const { return type_; }
  bool IsValid() const;

  const KeyArray& Array() const {
    DCHECK_EQ(type_, kArrayType);
    return array_;
  }

  scoped_refptr<SharedBuffer> Binary() const {
    DCHECK_EQ(type_, kBinaryType);
    return binary_;
  }

  const String& GetString() const {
    DCHECK_EQ(type_, kStringType);
    return string_;
  }

  double Date() const {
    DCHECK_EQ(type_, kDateType);
    return number_;
  }

  double Number() const {
    DCHECK_EQ(type_, kNumberType);
    return number_;
  }

  int Compare(const IDBKey* other) const;
  bool IsLessThan(const IDBKey* other) const;
  bool IsEqual(const IDBKey* other) const;
  size_t SizeEstimate() const { return size_estimate_; }

  // Returns a new key array with invalid keys and duplicates removed.
  //
  // The items in the key array are moved out of the given IDBKey, which must be
  // an array. For this reason, the method is a static method that receives its
  // argument via an std::unique_ptr.
  //
  // The return value will be pasesd to the backing store, which requires
  // Web types. Returning the correct types directly avoids copying later on
  // (wasted CPU cycles and code size).
  static WebVector<WebIDBKey> ToMultiEntryArray(
      std::unique_ptr<IDBKey> array_key);

 private:
  DISALLOW_COPY_AND_ASSIGN(IDBKey);

  IDBKey();
  IDBKey(Type type, double number);
  explicit IDBKey(const class String& value);
  explicit IDBKey(scoped_refptr<SharedBuffer> value);
  explicit IDBKey(KeyArray key_array);

  Type type_;
  KeyArray array_;
  scoped_refptr<SharedBuffer> binary_;
  const class String string_;
  const double number_ = 0;

  // Initialized in IDBKey constructors based on key type and value size (see
  // idb_key.cc).  Returned via SizeEstimate() and used in IndexedDB code to
  // verify that a given key is small enough to pass over IPC.
  size_t size_estimate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_KEY_H_
