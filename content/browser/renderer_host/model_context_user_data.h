// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MODEL_CONTEXT_USER_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_MODEL_CONTEXT_USER_DATA_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

// `ModelContextUserData` tracks registered tools and handles communication with
// `blink::ModelContext` It is created on-demand when the renderer first
// accesses `navigator.modelContext`. If it does not exist for a frame, it means
// that frame has not used the API yet and does not need to receive broadcasts
// like `toolchange`.
class CONTENT_EXPORT ModelContextUserData
    : public blink::mojom::ModelContextHost,
      public DocumentUserData<ModelContextUserData> {
 public:
  explicit ModelContextUserData(RenderFrameHost* rfh);
  ~ModelContextUserData() override;

  static void Bind(
      RenderFrameHost* rfh,
      mojo::PendingReceiver<blink::mojom::ModelContextHost> receiver);

  // blink::mojom::ModelContextHost implementation:
  void BindModelContext(
      mojo::PendingRemote<blink::mojom::ModelContext> model_context) override;
  void RegisterScriptTool(blink::mojom::ScriptToolPtr tool) override;
  void UnregisterScriptTool(const std::string& name) override;
  void GetScriptTools(GetScriptToolsCallback callback) override;

  std::vector<blink::mojom::ScriptToolPtr>& script_tools() {
    return script_tools_;
  }

 private:
  friend class DocumentUserData<ModelContextUserData>;

  DOCUMENT_USER_DATA_KEY_DECL();

  // Iterates over all documents in the frame tree and notifies them that the
  // list of available tools has changed, provided their origin matches one in
  // `exposed_origins` for the tool.
  void NotifyToolChange(const std::vector<url::Origin>& exposed_origins);

  mojo::Receiver<blink::mojom::ModelContextHost> receiver_{this};
  mojo::Remote<blink::mojom::ModelContext> model_context_remote_;

  std::vector<blink::mojom::ScriptToolPtr> script_tools_;

  base::WeakPtrFactory<ModelContextUserData> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MODEL_CONTEXT_USER_DATA_H_
