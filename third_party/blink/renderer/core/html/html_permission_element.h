// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT HTMLPermissionElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLPermissionElement(Document&);

  ~HTMLPermissionElement() override;

  const AtomicString& GetType() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(resolved, kResolved)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(dismissed, kDismissed)

  // ScriptWrappable implements
  void Trace(Visitor*) const override;

  // Given an input type, return permissions list. This method is for testing
  // only.
  static Vector<mojom::blink::PermissionDescriptorPtr>
  ParsePermissionDescriptorsForTesting(const AtomicString& type);

 private:
  // Ensure there is a connection to the permission service and return it.
  mojom::blink::PermissionService* GetPermissionService();
  void OnPermissionServiceConnectionFailed();

  // blink::Element implements
  void AttributeChanged(const AttributeModificationParams& params) override;

  // blink::Node override.
  void DefaultEventHandler(Event&) override;

  // Trigger permissions requesting in browser side by calling mojo
  // PermissionService's API
  void RequestPageEmbededPermissions();

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;

  AtomicString type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
