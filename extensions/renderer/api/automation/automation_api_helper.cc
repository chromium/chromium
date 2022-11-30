// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/automation/automation_api_helper.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace extensions {

AutomationApiHelper::AutomationApiHelper(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  DCHECK(render_frame->GetWebFrame()->IsOutermostMainFrame());
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::AutomationQuery>(
          base::BindRepeating(&AutomationApiHelper::BindAutomationQueryReceiver,
                              base::Unretained(this)));
}

AutomationApiHelper::~AutomationApiHelper() = default;

// Since this function is called after RenderFrame destruction, RenderFrame
// cannot be used to remove an interface binder from the registry.
void AutomationApiHelper::OnDestruct() {
  receivers_.Clear();
  delete this;
}

void AutomationApiHelper::BindAutomationQueryReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutomationQuery> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AutomationApiHelper::QuerySelector(int32_t acc_obj_id,
                                        const std::string& selector,
                                        QuerySelectorCallback callback) {
  // ExtensionMsg_AutomationQuerySelector should only be sent to an active view.
  DCHECK(render_frame()->IsMainFrame());

  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  if (document.IsNull()) {
    std::move(callback).Run(
        0, extensions::mojom::AutomationQueryError::kNoDocument);
    return;
  }
  blink::WebNode start_node = document;
  if (acc_obj_id > 0) {
    blink::WebAXObject start_acc_obj =
        blink::WebAXObject::FromWebDocumentByID(document, acc_obj_id);
    if (start_acc_obj.IsNull()) {
      std::move(callback).Run(
          0, extensions::mojom::AutomationQueryError::kNodeDestroyed);
      return;
    }

    start_node = start_acc_obj.GetNode();
    while (start_node.IsNull()) {
      start_acc_obj = start_acc_obj.ParentObject();
      start_node = start_acc_obj.GetNode();
    }
  }
  blink::WebString web_selector = blink::WebString::FromUTF8(selector);

  // Returns first match that has an attached, unignored node, otherwise null.
  blink::WebVector<blink::WebElement> all_matches =
      start_node.QuerySelectorAll(web_selector);
  int result_acc_obj_id = ui::kInvalidAXNodeID;
  for (const blink::WebElement& match : all_matches) {
    auto result_acc_obj = blink::WebAXObject::FromWebNode(match);
    if (!result_acc_obj.IsDetached() &&
        !result_acc_obj.AccessibilityIsIgnored()) {
      // Found unignored WebAXObject.
      result_acc_obj_id = result_acc_obj.AxID();
      break;
    }
  }
  std::move(callback).Run(result_acc_obj_id,
                          extensions::mojom::AutomationQueryError::kNone);
}

}  // namespace extensions
