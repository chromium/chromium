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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_KEY_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_KEY_H_

#include <memory>

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

class IDBKey;
class WebIDBKeyView;

// Minimal interface for iterating over an IndexedDB array key.
//
// See WebIDBKeyView for the rationale behind this class' existence.
class WebIDBKeyArrayView {
 public:
  BLINK_EXPORT size_t size() const;

  BLINK_EXPORT WebIDBKeyView operator[](size_t index) const;

 private:
  // Only WebIDBKeyView can vend WebIDBArrayKeyView instances.
  friend class WebIDBKeyView;
  explicit WebIDBKeyArrayView(const IDBKey* idb_key) : private_(idb_key) {}

  const IDBKey* const private_;
};

// Non-owning reference to an IndexedDB key.
//
// The Blink object wrapped by WebIDBKey is immutable, so WebIDBKeyView
// instances are implicitly const references.
//
// Having both WebIDBKeyView and WebIDBKey is extra complexity, and we pay this
// price to avoid unnecessary memory copying. Specifically, WebIDBKeyView is
// used to issue requests to the IndexedDB backing store.
//
// For example, IDBCursor.update() must send the cursor's primary key to the
// backing store. IDBCursor cannot give up the ownership of its primary key,
// because it might need to satisfy further update() or delete() calls.
class WebIDBKeyView {
 public:
  WebIDBKeyView(const WebIDBKeyView&) noexcept = default;

  explicit WebIDBKeyView(const IDBKey* idb_key) noexcept : private_(idb_key) {}

  BLINK_EXPORT WebIDBKeyType KeyType() const;

  BLINK_EXPORT bool IsValid() const;

  // Only valid for ArrayType.
  //
  // The caller is responsible for ensuring that the WebIDBKeyView is valid for
  // the lifetime of the returned WeIDBKeyArrayView.
  BLINK_EXPORT const WebIDBKeyArrayView ArrayView() const {
    return WebIDBKeyArrayView(private_);
  }

  // Only valid for BinaryType.
  BLINK_EXPORT WebData Binary() const;

  // Only valid for StringType.
  BLINK_EXPORT WebString String() const;

  // Only valid for DateType.
  BLINK_EXPORT double Date() const;

  // Only valid for NumberType.
  BLINK_EXPORT double Number() const;

  BLINK_EXPORT size_t SizeEstimate() const;

  // TODO(cmp): Ensure |private_| can never be null.
  //
  // SizeEstimate() can be called when |private_| is null.  That happens if and
  // only if the |WebIDBKey| instance is created using WebIDBKey::CreateNull().
  //
  // Eventually, WebIDBKey::CreateNull() will change so that case will lead to
  // a non-null |private_|.  At that time, this null check can change to a
  // DCHECK that |private_| is not null and the special null case handling can
  // be removed.
  BLINK_EXPORT bool IsNull() const { return !private_; }

 private:
  const IDBKey* const private_;
};

// Move-only handler that owns an IndexedDB key.
//
// The wrapped Blink object wrapped is immutable while it is owned by the
// WebIDBKey.
//
// Having both WebIDBKeyView and WebIDBKeyArrayView is extra complexity, and we
// pay this price to avoid unnecessary memory copying. Specifically, WebIDBKey
// is used to receive data from the IndexedDB backing store. Once constructed, a
// WebIDBKey is moved through the layer cake until the underlying Blink object
// ends up at its final destination.
class WebIDBKey {
 public:
  BLINK_EXPORT static WebIDBKey CreateArray(WebVector<WebIDBKey>);
  BLINK_EXPORT static WebIDBKey CreateBinary(const WebData&);
  BLINK_EXPORT static WebIDBKey CreateString(const WebString&);
  BLINK_EXPORT static WebIDBKey CreateDate(double);
  BLINK_EXPORT static WebIDBKey CreateNumber(double);
  BLINK_EXPORT static WebIDBKey CreateInvalid();
  BLINK_EXPORT static WebIDBKey CreateNull() noexcept { return WebIDBKey(); }

  // The default constructor must not be used explicitly.
  // It is only provided for WebVector and Mojo's use.
  BLINK_EXPORT WebIDBKey() noexcept;

  BLINK_EXPORT WebIDBKey(WebIDBKey&&) noexcept;
  BLINK_EXPORT WebIDBKey& operator=(WebIDBKey&&) noexcept;

  // TODO(cmp): Remove copy and assignment constructors when Vector<->WebVector
  //            conversions are no longer needed.
  BLINK_EXPORT WebIDBKey(const WebIDBKey& rkey);
  BLINK_EXPORT WebIDBKey& operator=(const WebIDBKey& rkey);

  BLINK_EXPORT ~WebIDBKey();

  BLINK_EXPORT WebIDBKeyView View() const {
    return WebIDBKeyView(private_.get());
  }

  BLINK_EXPORT size_t SizeEstimate() const { return View().SizeEstimate(); }

#if INSIDE_BLINK
  explicit WebIDBKey(std::unique_ptr<IDBKey>) noexcept;
  WebIDBKey& operator=(std::unique_ptr<IDBKey>) noexcept;
  operator IDBKey*() const noexcept { return private_.get(); }

  std::unique_ptr<IDBKey> ReleaseIdbKey() noexcept {
    return std::move(private_);
  }
#endif  // INSIDE_BLINK

 private:

  std::unique_ptr<IDBKey> private_;
};

using WebIDBIndexKeys = std::pair<int64_t, WebVector<WebIDBKey>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_WEB_IDB_KEY_H_
