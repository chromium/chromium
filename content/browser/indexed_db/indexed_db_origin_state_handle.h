// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ORIGIN_STATE_HANDLE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ORIGIN_STATE_HANDLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {
class IndexedDBOriginState;

// This handle tells the IndexedDBOriginState that there is still something
// using the origin, and the IndexedDBOriginState won't close until all
// handles are destroyed. Destroying this handle can cause the origin state to
// synchronously destruct, which modifies the |factories_per_origin_| map in
// IndexedDBFactoryImpl.
class CONTENT_EXPORT IndexedDBOriginStateHandle {
 public:
  IndexedDBOriginStateHandle();
  explicit IndexedDBOriginStateHandle(
      base::WeakPtr<IndexedDBOriginState> origin_state);
  IndexedDBOriginStateHandle(IndexedDBOriginStateHandle&&);
  IndexedDBOriginStateHandle& operator=(IndexedDBOriginStateHandle&&);
  ~IndexedDBOriginStateHandle();

  bool IsHeld() const;

  void Release();

  // Returns null if the factory was destroyed, which should only happen on
  // context destruction.
  IndexedDBOriginState* origin_state() { return origin_state_.get(); }

 private:
  base::WeakPtr<IndexedDBOriginState> origin_state_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBOriginStateHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_ORIGIN_STATE_HANDLE_H_
