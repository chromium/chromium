// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_

#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "url/origin.h"

namespace storage {
class SharedStorageManager;
}

namespace content {

class RenderFrameHost;
class SharedStorageWorkletHost;

// Handle renderer-initiated shared storage access and worklet operations. The
// worklet operations (i.e. addModule and runOperation) will be dispatched to
// the `SharedStorageWorkletHost` to be handled.
class SharedStorageDocumentServiceImpl final
    : public DocumentUserData<SharedStorageDocumentServiceImpl>,
      public blink::mojom::SharedStorageDocumentService {
 public:
  ~SharedStorageDocumentServiceImpl() final;

  void Bind(mojo::PendingAssociatedReceiver<
            blink::mojom::SharedStorageDocumentService> receiver);

  // blink::mojom::SharedStorageDocumentService.
  void AddModuleOnWorklet(const GURL& script_source_url,
                          AddModuleOnWorkletCallback callback) override;
  void RunOperationOnWorklet(
      const std::string& name,
      const std::vector<uint8_t>& serialized_data) override;
  void RunURLSelectionOperationOnWorklet(
      const std::string& name,
      const std::vector<GURL>& urls,
      const std::vector<uint8_t>& serialized_data,
      RunURLSelectionOperationOnWorkletCallback callback) override;
  void SharedStorageSet(const std::u16string& key,
                        const std::u16string& value,
                        bool ignore_if_present) override;
  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value) override;
  void SharedStorageDelete(const std::u16string& key) override;
  void SharedStorageClear() override;

  base::WeakPtr<SharedStorageDocumentServiceImpl> GetWeakPtr();

 private:
  friend class DocumentUserData;

  explicit SharedStorageDocumentServiceImpl(RenderFrameHost*);

  SharedStorageWorkletHost* GetSharedStorageWorkletHost();

  storage::SharedStorageManager* GetSharedStorageManager();

  mojo::AssociatedReceiver<blink::mojom::SharedStorageDocumentService>
      receiver_{this};

  DOCUMENT_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<SharedStorageDocumentServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
