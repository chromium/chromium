// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_DOCUMENT_USER_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_DOCUMENT_USER_DATA_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

// `ScriptToolDocumentUserData` tracks registered tools and handles
// communication with `blink::ModelContext`. It is created on-demand when the
// renderer first accesses `document.modelContext`. If it does not exist for a
// frame, it means that frame has not used the API yet and does not need to
// receive broadcasts like `toolchange`.
class CONTENT_EXPORT ScriptToolDocumentUserData
    : public blink::mojom::ModelContextHost,
      public DocumentUserData<ScriptToolDocumentUserData> {
 public:
  static bool IsWebMCPEnabled(RenderFrameHost& rfh);

  explicit ScriptToolDocumentUserData(RenderFrameHost* rfh);
  ~ScriptToolDocumentUserData() override;

  static void Bind(
      RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<blink::mojom::ModelContextHost> receiver);

  // blink::mojom::ModelContextHost implementation:
  void BindModelContext(
      mojo::PendingAssociatedRemote<blink::mojom::ModelContext> model_context)
      override;
  void RegisterScriptTool(blink::mojom::ScriptToolPtr tool,
                          RegisterScriptToolCallback callback) override;
  void UnregisterScriptTool(const blink::ScriptToolToken& tool_id) override;
  void GetScriptTools(const std::vector<url::Origin>& from_origins,
                      GetScriptToolsCallback callback) override;
  void ExecuteRemoteScriptTool(
      const blink::ToolInvocationToken& invocation_id,
      const blink::ScriptToolToken& tool_id,
      const blink::FrameToken& tool_owner_frame_token,
      const url::Origin& expected_target_origin,
      const std::string& input_arguments,
      ExecuteRemoteScriptToolCallback callback) override;
  void CancelRemoteScriptTool(
      const blink::ToolInvocationToken& invocation_id) override;
  void SetScriptToolsReady(bool ready) override;
  void RequestContinuationToken(
      const blink::ToolInvocationToken& invocation_id,
      RequestContinuationTokenCallback callback) override;
  void RedeemContinuationToken(
      const blink::ContinuationToken& token,
      RedeemContinuationTokenCallback callback) override;

  std::vector<blink::mojom::ScriptToolPtr>& script_tools() {
    return script_tools_;
  }

  bool script_tools_waiting() const { return script_tools_waiting_; }

 private:
  friend class DocumentUserData<ScriptToolDocumentUserData>;
  friend class ScriptToolPageUserData;
  friend class ScriptToolContinuationManager;

  DOCUMENT_USER_DATA_KEY_DECL();

  // Iterates over all documents in the frame tree and notifies them that the
  // list of available tools has changed, provided their origin matches one in
  // `exposed_origins` for the tool.
  void NotifyToolChange(const std::vector<url::Origin>& exposed_origins);

  mojo::AssociatedReceiver<blink::mojom::ModelContextHost> receiver_{this};
  mojo::AssociatedRemote<blink::mojom::ModelContext> model_context_remote_;

  std::vector<blink::mojom::ScriptToolPtr> script_tools_;

  // Whether the document is waiting for script tools to be ready
  // via unsettled Promises passed to modelContext.waitForToolsUntil().
  bool script_tools_waiting_ = false;

  base::WeakPtrFactory<ScriptToolDocumentUserData> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SCRIPT_TOOLS_SCRIPT_TOOL_DOCUMENT_USER_DATA_H_
