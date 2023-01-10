// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_

#include "content/common/content_export.h"
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
class SharedStorageWorkletHostManager;

extern CONTENT_EXPORT const char kSharedStorageDisabledMessage[];
extern CONTENT_EXPORT const char kSharedStorageSelectURLDisabledMessage[];
extern CONTENT_EXPORT const char kSharedStorageAddModuleDisabledMessage[];
extern CONTENT_EXPORT const char kSharedStorageSelectURLLimitReachedMessage[];

// Handle renderer-initiated shared storage access and worklet operations. The
// worklet operations (i.e. `addModule()`, `selectURL()`, `run()`) will be
// dispatched to the `SharedStorageWorkletHost` to be handled.
class CONTENT_EXPORT SharedStorageDocumentServiceImpl final
    : public DocumentUserData<SharedStorageDocumentServiceImpl>,
      public blink::mojom::SharedStorageDocumentService {
 public:
  // If true, allows operations to bypass the permission check in
  // `IsSharedStorageAllowed()` for testing, in order to simulate the situation
  // where permission is allowed at the stage where `run()` is called but
  // becomes disallowed when subsequent operations are called from inside the
  // worklet.
  static bool& GetBypassIsSharedStorageAllowedForTesting();

  ~SharedStorageDocumentServiceImpl() final;

  const url::Origin& main_frame_origin() const { return main_frame_origin_; }

  std::string main_frame_id() const { return main_frame_id_; }

  void Bind(mojo::PendingAssociatedReceiver<
            blink::mojom::SharedStorageDocumentService> receiver);

  // blink::mojom::SharedStorageDocumentService.
  void AddModuleOnWorklet(const GURL& script_source_url,
                          AddModuleOnWorkletCallback callback) override;
  void RunOperationOnWorklet(const std::string& name,
                             const std::vector<uint8_t>& serialized_data,
                             RunOperationOnWorkletCallback callback) override;
  void RunURLSelectionOperationOnWorklet(
      const std::string& name,
      std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
          urls_with_metadata,
      const std::vector<uint8_t>& serialized_data,
      RunURLSelectionOperationOnWorkletCallback callback) override;
  void SharedStorageSet(const std::u16string& key,
                        const std::u16string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override;
  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value,
                           SharedStorageAppendCallback callback) override;
  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override;
  void SharedStorageClear(SharedStorageClearCallback callback) override;

  base::WeakPtr<SharedStorageDocumentServiceImpl> GetWeakPtr();

 private:
  friend class DocumentUserData;

  static bool& GetBypassIsSharedStorageAllowed();

  explicit SharedStorageDocumentServiceImpl(RenderFrameHost*);

  SharedStorageWorkletHostManager* GetSharedStorageWorkletHostManager();

  SharedStorageWorkletHost* GetSharedStorageWorkletHost();

  storage::SharedStorageManager* GetSharedStorageManager();

  bool IsSharedStorageAllowed();

  bool IsSharedStorageSelectURLAllowed();

  bool IsSharedStorageAddModuleAllowed();

  std::string SerializeLastCommittedOrigin() const;

  mojo::AssociatedReceiver<blink::mojom::SharedStorageDocumentService>
      receiver_{this};

  // To avoid race conditions associated with top frame navigations, we need to
  // save the value of the main frame origin in the constructor.
  const url::Origin main_frame_origin_;

  // The DevTools frame token for the main frame, to be used by notifications
  // to DevTools.
  const std::string main_frame_id_;

  DOCUMENT_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<SharedStorageDocumentServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
