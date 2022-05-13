// Copyright 2014 The Chromium Authors. All rights reserved.
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
}

AutomationApiHelper::~AutomationApiHelper() {
}

bool AutomationApiHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutomationApiHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AutomationQuerySelector, OnQuerySelector)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutomationApiHelper::OnDestruct() {
  delete this;
}

void AutomationApiHelper::OnQuerySelector(int request_id,
                                          int acc_obj_id,
                                          const std::u16string& selector) {
  // ExtensionMsg_AutomationQuerySelector should only be sent to an active view.
  DCHECK(render_frame()->IsMainFrame());

  ExtensionHostMsg_AutomationQuerySelector_Error error;

  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  if (document.IsNull()) {
    error.value = ExtensionHostMsg_AutomationQuerySelector_Error::kNoDocument;
    Send(new ExtensionHostMsg_AutomationQuerySelector_Result(
        routing_id(), request_id, error, 0));
    return;
  }
  blink::WebNode start_node = document;
  if (acc_obj_id > 0) {
    blink::WebAXObject start_acc_obj =
        blink::WebAXObject::FromWebDocumentByID(document, acc_obj_id);
    if (start_acc_obj.IsNull()) {
      error.value =
          ExtensionHostMsg_AutomationQuerySelector_Error::kNodeDestroyed;
      Send(new ExtensionHostMsg_AutomationQuerySelector_Result(
          routing_id(), request_id, error, 0));
      return;
    }

    start_node = start_acc_obj.GetNode();
    while (start_node.IsNull()) {
      start_acc_obj = start_acc_obj.ParentObject();
      start_node = start_acc_obj.GetNode();
    }
  }
  blink::WebString web_selector = blink::WebString::FromUTF16(selector);

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
  Send(new ExtensionHostMsg_AutomationQuerySelector_Result(
      routing_id(), request_id, error, result_acc_obj_id));
}

}  // namespace extensions
