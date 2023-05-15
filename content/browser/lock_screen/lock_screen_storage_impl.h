// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOCK_SCREEN_LOCK_SCREEN_STORAGE_IMPL_H_
#define CONTENT_BROWSER_LOCK_SCREEN_LOCK_SCREEN_STORAGE_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "content/public/browser/lock_screen_storage.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/lock_screen/lock_screen.mojom.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {

class LockScreenStorageHelper;

// Global storage for lock screen data stored by websites. This isn't
// BrowserContext keyed because there is only ever one lock screen profile (the
// primary user's BrowserContext).
class CONTENT_EXPORT LockScreenStorageImpl : public LockScreenStorage {
 public:
  static LockScreenStorageImpl* GetInstance();

  LockScreenStorageImpl(const LockScreenStorageImpl&) = delete;
  LockScreenStorageImpl& operator=(const LockScreenStorageImpl&) = delete;
  virtual ~LockScreenStorageImpl();

  // LockScreenStorage overrides.
  void Init(content::BrowserContext* browser_context,
            const base::FilePath& base_path) override;

  void GetKeys(const url::Origin& origin,
               blink::mojom::LockScreenService::GetKeysCallback callback);
  void SetData(const url::Origin& origin,
               const std::string& key,
               const std::string& data,
               blink::mojom::LockScreenService::SetDataCallback);

  // Whether the BrowserContext is allowed to store/retrieve lock screen data.
  bool IsAllowedBrowserContext(content::BrowserContext* profile);

 private:
  LockScreenStorageImpl();

  void OnGetKeys(blink::mojom::LockScreenService::GetKeysCallback callback,
                 const std::vector<std::string>& result);
  void OnSetData(blink::mojom::LockScreenService::SetDataCallback callback,
                 bool success);

  // Reinitialize the storage for testing.
  void InitForTesting(content::BrowserContext* browser_context,
                      const base::FilePath& base_path);

  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_ =
      nullptr;
  base::SequenceBound<LockScreenStorageHelper> helper_;

  base::WeakPtrFactory<LockScreenStorageImpl> weak_factory_{this};

  friend class LockScreenServiceImplBrowserTest;
  friend struct base::DefaultSingletonTraits<LockScreenStorageImpl>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOCK_SCREEN_LOCK_SCREEN_STORAGE_IMPL_H_
