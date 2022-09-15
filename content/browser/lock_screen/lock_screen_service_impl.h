// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOCK_SCREEN_LOCK_SCREEN_SERVICE_IMPL_H_
#define CONTENT_BROWSER_LOCK_SCREEN_LOCK_SCREEN_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/lock_screen/lock_screen.mojom.h"

namespace content {

class LockScreenStorageImpl;
class RenderFrameHost;

class CONTENT_EXPORT LockScreenServiceImpl
    : public DocumentService<blink::mojom::LockScreenService> {
 public:
  LockScreenServiceImpl(const LockScreenServiceImpl&) = delete;
  LockScreenServiceImpl& operator=(const LockScreenServiceImpl&) = delete;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::LockScreenService> receiver);

  // blink::mojom::LockScreenService:
  void GetKeys(GetKeysCallback callback) override;
  void SetData(const std::string& key,
               const std::string& data,
               SetDataCallback) override;

 private:
  friend class LockScreenServiceImplBrowserTest;

  explicit LockScreenServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::LockScreenService> receiver);

  // |this| can only be destructed as a DocumentService.
  ~LockScreenServiceImpl() override;

  bool IsAllowed();

  raw_ptr<LockScreenStorageImpl> lock_screen_storage_;

  base::WeakPtrFactory<LockScreenServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOCK_SCREEN_LOCK_SCREEN_SERVICE_IMPL_H_
