// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
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

  void Trace(Visitor*) const override;

  void AttachLayoutTree(AttachContext& context) override;

  bool granted() const { return permissions_granted_; }

  // Given an input type, return permissions list. This method is for testing
  // only.
  static Vector<mojom::blink::PermissionDescriptorPtr>
  ParsePermissionDescriptorsForTesting(const AtomicString& type);

 private:
  enum class DisableReason {
    kRecentlyAttachedToDOM,
  };

  // Ensure there is a connection to the permission service and return it.
  mojom::blink::PermissionService* GetPermissionService();
  void OnPermissionServiceConnectionFailed();

  // blink::Element implements
  void AttributeChanged(const AttributeModificationParams& params) override;
  void DidAddUserAgentShadowRoot(ShadowRoot&) override;

  // blink::Node override.
  void DefaultEventHandler(Event&) override;

  // Trigger permissions requesting in browser side by calling mojo
  // PermissionService's API
  void RequestPageEmbededPermissions();

  // Checks whether clicking is enabled at the moment. Clicking is disabled if
  // either:
  // 1) |DisableClickingIndefinitely| has been called and |EnableClicking| has
  // not been called (yet).
  // 2) |DisableClickingTemporarily| has been called and the specified duration
  // has not yet elapsed.
  //
  // Clicking can be disabled for multiple reasons simultaneously, and it needs
  // to be re-enabled (or the temporary duration to elapse) for each independent
  // reason before it becomes enabled again.
  bool IsClickingEnabled();

  // Disables clicking indefinitely for |reason|. |EnableClicking| for the same
  // reason needs to be called to re-enable it.
  void DisableClickingIndefinitely(DisableReason reason);

  // Disables clicking temporarily for |reason|. |EnableClicking| can be called
  // to re-enable clicking, or the duration needs to elapse.
  void DisableClickingTemporarily(DisableReason reason,
                                  const base::TimeDelta& duration);

  // Removes any existing (temporary or indefinite) disable reasons.
  void EnableClicking(DisableReason reason);

  // Callback triggered when permission is decided from browser side
  void OnEmbededPermissionsDecided(
      mojom::blink::EmbeddedPermissionControlResult result);

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;

  AtomicString type_;

  // Holds reasons for which clicking is currently disabled (if any). Each
  // entry will have an expiration time associated with it, which can be
  // |base::TimeTicks::Max()| if it's indefinite.
  HashMap<DisableReason, base::TimeTicks> clicking_disabled_reasons_;

  Member<HTMLDivElement> inner_element_;
  Member<HTMLSpanElement> permission_text_;

  // Set to true only if all the corresponding permissions (from `type`
  // attribute) are granted.
  bool permissions_granted_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
