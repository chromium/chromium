// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
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

// Will conditionally combine an `input_message` with a `debug_message`
// containing additional details if the
// `blink::features::kSharedStorageDebugDisabledMessage` feature param is true,
// returning `input_message` otherwise.
std::string GetSharedStorageErrorMessage(const std::string& debug_message,
                                         const std::string& input_message);

// Handle renderer-initiated shared storage access and worklet operations. The
// worklet operations (i.e. `addModule()`, `selectURL()`, `run()`) will be
// dispatched to the `SharedStorageWorkletHost` to be handled.
class CONTENT_EXPORT SharedStorageDocumentServiceImpl final
    : public DocumentUserData<SharedStorageDocumentServiceImpl>,
      public blink::mojom::SharedStorageDocumentService {
 public:
  ~SharedStorageDocumentServiceImpl() final;

  const url::Origin& main_frame_origin() const { return main_frame_origin_; }

  int main_frame_id() const { return main_frame_id_; }

  void Bind(mojo::PendingAssociatedReceiver<
            blink::mojom::SharedStorageDocumentService> receiver);

  // blink::mojom::SharedStorageDocumentService.
  void CreateWorklet(
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      const std::vector<blink::mojom::OriginTrialFeature>&
          origin_trial_features,
      mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
          worklet_host,
      CreateWorkletCallback callback) override;
  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override;
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

  explicit SharedStorageDocumentServiceImpl(RenderFrameHost*);

  SharedStorageWorkletHostManager* GetSharedStorageWorkletHostManager();

  SharedStorageWorkletHost* GetSharedStorageWorkletHost();

  storage::SharedStorageManager* GetSharedStorageManager();

  bool IsSharedStorageAllowed(std::string* out_debug_message = nullptr);

  bool IsSharedStorageAddModuleAllowed(
      std::string* out_debug_message = nullptr);

  std::string SerializeLastCommittedOrigin() const;

  mojo::AssociatedReceiver<blink::mojom::SharedStorageDocumentService>
      receiver_{this};

  bool create_worklet_called_ = false;

  // To avoid race conditions associated with top frame navigations, we need to
  // save the value of the main frame origin in the constructor.
  const url::Origin main_frame_origin_;

  // The FrameTreeNodeId for the main frame, to be used by notifications
  // to DevTools. (DevTools will convert this to a DevTools frame token.)
  const int main_frame_id_;

  DOCUMENT_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<SharedStorageDocumentServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_DOCUMENT_SERVICE_IMPL_H_
