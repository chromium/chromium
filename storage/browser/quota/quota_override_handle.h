// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_OVERRIDE_HANDLE_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_OVERRIDE_HANDLE_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "url/origin.h"

namespace storage {

class QuotaManagerProxy;

// Used by DevTools clients and exposes the API to override and/or
// manage an active override for storage quota on a per-origin basis.
// QuotaOverrideHandle instances are owned by StorageHandler (DevTools client),
// and each DevTools session will have at most 1 instance.
// This class is not thread-safe. An instance must always be used from the same
// sequence. However, this sequence can be different from the one that
// QuotaManager lives on.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaOverrideHandle {
 public:
  explicit QuotaOverrideHandle(scoped_refptr<QuotaManagerProxy> quota_manager);
  ~QuotaOverrideHandle();
  QuotaOverrideHandle(const QuotaOverrideHandle&) = delete;

  void OverrideQuotaForOrigin(url::Origin origin,
                              base::Optional<int64_t> quota_size,
                              base::OnceClosure callback);

 private:
  void GetUniqueId();
  void DidGetUniqueId();
  void DidGetOverrideHandleId(int id);

  base::Optional<int> id_;
  std::vector<base::OnceClosure> override_callback_queue_;
  scoped_refptr<QuotaManagerProxy> quota_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<QuotaOverrideHandle> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_OVERRIDE_HANDLE_
