// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_QUOTA_CONTEXT_H_
#define CONTENT_BROWSER_QUOTA_QUOTA_CONTEXT_H_

#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "content/browser/quota/quota_change_dispatcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "storage/browser/quota/quota_settings.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_manager_host.mojom-forward.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class QuotaManager;
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

class QuotaManagerHost;

// Owns the Quota sub-system for a StoragePartition.
//
// Each StoragePartition instance owns exactly one instance of QuotaContext.
//
// The reference counting is an unfortunate consequence of the need to interact
// with QuotaManager on the I/O thread, and will probably disappear when
// QuotaManager moves to the Storage Service.
class QuotaContext : public base::RefCountedDeleteOnSequence<QuotaContext> {
 public:
  QuotaContext(
      bool is_incognito,
      const base::FilePath& profile_path,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      storage::GetQuotaSettingsFunc get_settings_function);

  QuotaContext(const QuotaContext&) = delete;
  QuotaContext& operator=(const QuotaContext&) = delete;

  storage::QuotaManager* quota_manager() { return quota_manager_.get(); }

  // Must be called from the UI thread.
  void BindQuotaManagerHost(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver);

  void OverrideQuotaManagerForTesting(
      scoped_refptr<storage::QuotaManager> new_manager);

 private:
  friend class base::RefCountedDeleteOnSequence<QuotaContext>;
  friend class base::DeleteHelper<QuotaContext>;

  ~QuotaContext();

  void BindQuotaManagerHostOnIOThread(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver);

  // QuotaManager runs on the IO thread, so mojo receivers must be bound there.
  const scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  // Owning reference for the QuotaChangeDispatcher.
  scoped_refptr<QuotaChangeDispatcher> quota_change_dispatcher_;

  // Owning reference for the QuotaManager.
  //
  // This is not const because of OverrideQuotaManagerForTesting().
  scoped_refptr<storage::QuotaManager> quota_manager_;

  // Only accessed on the IO thread.
  mojo::ReceiverSet<blink::mojom::QuotaManagerHost,
                    std::unique_ptr<QuotaManagerHost>>
      receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_QUOTA_CONTEXT_H_
