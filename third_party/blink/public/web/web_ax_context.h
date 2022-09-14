// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"

namespace ui {
class AXMode;
}  // namespace ui

namespace blink {

class AXContext;

// An instance of this class, while kept alive, enables accessibility
// support for the given document.
class BLINK_EXPORT WebAXContext {
 public:
  explicit WebAXContext(WebDocument document, const ui::AXMode& mode);
  ~WebAXContext();

  // Returns the root element of the document's accessibility tree.
  WebAXObject Root() const;

  const ui::AXMode& GetAXMode() const;

  void SetAXMode(const ui::AXMode&) const;

  void ResetSerializer();

  // Get a new AXID that's not used by any accessibility node in this process,
  // for when the client needs to insert additional nodes into the accessibility
  // tree.
  int GenerateAXID() const;

  // Retrieves a vector of all WebAXObjects in this document whose
  // bounding boxes may have changed since the last query. Sends that vector
  // via mojo to the browser process.
  void SerializeLocationChanges() const;

  // Searches the accessibility tree for plugin's root object and returns it.
  // Returns an empty WebAXObject if no root object is present.
  WebAXObject GetPluginRoot();

  void Freeze();

  void Thaw();

  bool SerializeEntireTree(bool exclude_offscreen,
                           size_t max_node_count,
                           base::TimeDelta timeout,
                           ui::AXTreeUpdate* response);

  void MarkAllImageAXObjectsDirty(
      ax::mojom::Action event_from_action);

 private:
  std::unique_ptr<AXContext> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_
