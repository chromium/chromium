// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_RUNTIME_MANAGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_RUNTIME_MANAGER_H_

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_worklet_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_select_url_fenced_frame_config_observer.h"
#include "content/test/content_browser_test_utils_internal.h"  // For `ConvertToRenderFrameHost()`
#include "content/test/fenced_frame_test_utils.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "net/base/schemeful_site.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "url/origin.h"

namespace content {

class TestSharedStorageRuntimeManager : public SharedStorageRuntimeManager {
 public:
  using SharedStorageRuntimeManager::SharedStorageRuntimeManager;

  explicit TestSharedStorageRuntimeManager(
      StoragePartitionImpl& storage_partition);

  ~TestSharedStorageRuntimeManager() override;

  std::unique_ptr<SharedStorageWorkletHost> CreateWorkletHostHelper(
      SharedStorageDocumentServiceImpl& document_service,
      const url::Origin& frame_origin,
      const url::Origin& data_origin,
      blink::mojom::SharedStorageDataOriginType data_origin_type,
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::SharedStorageWorkletCreationMethod creation_method,
      int worklet_ordinal_id,
      const std::vector<blink::mojom::OriginTrialFeature>&
          origin_trial_features,
      mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
          worklet_host,
      blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
          callback) override;

  // Precondition: there's only one eligible worklet host.
  TestSharedStorageWorkletHost* GetAttachedWorkletHost();

  std::vector<TestSharedStorageWorkletHost*> GetAttachedWorkletHosts();

  // Precondition: there's only one eligible worklet host.
  TestSharedStorageWorkletHost* GetKeepAliveWorkletHost();

  // Precondition: `frame` is associated with a
  // `SharedStorageDocumentServiceImpl`, and there's a single attached
  // `SharedStorageWorkletHost` for that document service.
  TestSharedStorageWorkletHost* GetAttachedWorkletHostForFrame(
      RenderFrameHost* frame);

  // Gets the host with `script_source_url` that was most recently attached for
  // `frame`'s document service. Returns nullptr if there is no such host.
  TestSharedStorageWorkletHost* GetLastAttachedWorkletHostForFrameWithScriptSrc(
      RenderFrameHost* frame,
      const GURL& script_src_url);

  // Precondition: `frame` is associated with a
  // `SharedStorageDocumentServiceImpl`.
  std::vector<TestSharedStorageWorkletHost*> GetAttachedWorkletHostsForFrame(
      RenderFrameHost* frame);

  // Returns a map of worklet ordinal IDs to worklet DevTools tokens for all of
  // the shared storage worklet hosts created up to that point of the test,
  // regardless of which of these hosts are still attached and/or alive.
  std::map<int, base::UnguessableToken>& GetCachedWorkletHostDevToolsTokens();

  void ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(
      bool should_defer_worklet_messages);

  size_t GetAttachedWorkletHostsCount();

  size_t GetKeepAliveWorkletHostsCount();

 private:
  bool should_defer_worklet_messages_ = false;

  // A map of worklet ordinal IDs to worklet DevTools Tokens.
  std::map<int, base::UnguessableToken> cached_worklet_host_devtools_tokens_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_TEST_SHARED_STORAGE_RUNTIME_MANAGER_H_
