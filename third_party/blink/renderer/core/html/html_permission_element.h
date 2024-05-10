// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT HTMLPermissionElement final
    : public HTMLElement,
      public mojom::blink::PermissionObserver,
      public mojom::blink::EmbeddedPermissionControlClient,
      public LocalFrameView::LifecycleNotificationObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLPermissionElement(Document&);

  ~HTMLPermissionElement() override;

  const AtomicString& GetType() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(resolve, kResolve)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(dismiss, kDismiss)

  void Trace(Visitor*) const override;

  void AttachLayoutTree(AttachContext& context) override;
  void DetachLayoutTree(bool performing_reattach) override;
  void Focus(const FocusParams& params) override;
  bool SupportsFocus(UpdateBehavior) const override;
  int DefaultTabIndex() const override;
  CascadeFilter GetCascadeFilter() const override;

  bool granted() const { return permissions_granted_; }

  // Given an input type, return permissions list. This method is for testing
  // only.
  static Vector<mojom::blink::PermissionDescriptorPtr>
  ParsePermissionDescriptorsForTesting(const AtomicString& type);

  const Member<HTMLSpanElement>& permission_text_span_for_testing() const {
    return permission_text_span_;
  }

  // HTMLElement overrides.
  bool IsHTMLPermissionElement() const final { return true; }

  bool IsFullyVisibleForTesting() const { return is_fully_visible_; }

 private:
  // TODO(crbug.com/1315595): remove this friend class once migration
  // to blink_unittests_v2 completes.
  friend class ClickingEnabledChecker;
  friend class RegistrationWaiter;

  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementClickingEnabledTest,
                           UnclickableBeforeRegistered);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementIntersectionTest,
                           IntersectionChanged);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementFencedFrameTest,
                           NotAllowedInFencedFrame);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementSimTest,
                           BlockedByMissingFrameAncestorsCSP);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementSimTest,
                           EnableClickingAfterDelay);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementLayoutChangeTest,
                           InvalidatePEPCAfterMove);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementLayoutChangeTest,
                           InvalidatePEPCAfterResize);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementLayoutChangeTest,
                           InvalidatePEPCAfterMoveContainer);
  FRIEND_TEST_ALL_PREFIXES(HTMLPemissionElementLayoutChangeTest,
                           InvalidatePEPCAfterTransformContainer);

  enum class DisableReason {
    // This element is temporarily disabled for a short period
    // (`kDefaultDisableTimeout`) after being attached to the layout tree.
    kRecentlyAttachedToLayoutTree,

    // This element is temporarily disabled for a short period
    // (`kDefaultDisableTimeout`) after its intersection status changed from
    // invisible to visible.
    kIntersectionChanged,

    // This element is disabled because of the element's style.
    kInvalidStyle,
  };

  // Translates `DisableReason` into strings, primarily used for logging
  // console messages.
  static String DisableReasonToString(DisableReason reason);

  // Ensure there is a connection to the permission service and return it.
  mojom::blink::PermissionService* GetPermissionService();
  void OnPermissionServiceConnectionFailed();

  // blink::Element implements
  void AttributeChanged(const AttributeModificationParams& params) override;
  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  void AdjustStyle(ComputedStyleBuilder& builder) override;
  void DidRecalcStyle(const StyleRecalcChange change) override;

  // blink::Node override.
  void DefaultEventHandler(Event&) override;

  // Trigger permissions requesting in browser side by calling mojo
  // PermissionService's API.
  void RequestPageEmbededPermissions();

  void RegisterPermissionObserver(
      const mojom::blink::PermissionDescriptorPtr& descriptor,
      mojom::blink::PermissionStatus current_status);

  // mojom::blink::PermissionObserver override.
  void OnPermissionStatusChange(mojom::blink::PermissionStatus status) override;

  // mojom::blink::EmbeddedPermissionControlClient override.
  void OnEmbeddedPermissionControlRegistered(
      bool allowed,
      const std::optional<Vector<mojom::blink::PermissionStatus>>& statuses)
      override;

  // Callback triggered when permission is decided from browser side.
  void OnEmbeddedPermissionsDecided(
      mojom::blink::EmbeddedPermissionControlResult result);

  // Verify whether the element has been registered in browser process by
  // checking `permission_status_map_`. This map is initially empty and is
  // populated only *after* the permission element has been registered in
  // browser process.
  bool IsRegisteredInBrowserProcess() const {
    return !permission_status_map_.empty();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

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

  // Similar to `EnableClicking`, calling this method can override any disabled
  // duration for a given reason, but after a delay. Does nothing if clicking is
  // not currently disabled for the specified reason.
  void EnableClickingAfterDelay(DisableReason reason,
                                const base::TimeDelta& delay);

  void UpdateAppearance();

  void UpdateText();

  void AddConsoleError(String error);
  void AddConsoleWarning(String warning);

  void OnIntersectionChanged(
      const HeapVector<Member<IntersectionObserverEntry>>& entries);

  bool IsStyleValid();

  // Returns an adjusted bounded length that takes in the site-provided length
  // and creates an expression-type length of the form:
  // `min|max(|length|, (|fit-content-size| *)? |bound|)`,
  // |is_lower_bound| determines whether `min` or `max` should be used.
  // |should_multiply_by_content_size| determines whether the |bound| is
  // multiplied by the permission element's content size or is simply a fixed
  // pixel length.

  // If |length| is not a specified length, it is ignored, together with
  // |is_lower_bound|.
  Length AdjustedBoundedLength(const Length& length,
                               float bound,
                               bool is_lower_bound,
                               bool should_multiply_by_content_size);

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;

  // Holds all `PermissionObserver` receivers connected with remotes in browser
  // process. Each of them corresponds to a permission observer of one
  // descriptor in `permission_descriptors_`.
  // This set uses `PermissionName` as context type. Once a receiver call is
  // triggered, we look into its name to determine which permission is changed.
  HeapMojoReceiverSet<mojom::blink::PermissionObserver,
                      HTMLPermissionElement,
                      HeapMojoWrapperMode::kWithContextObserver,
                      mojom::blink::PermissionName>
      permission_observer_receivers_;

  // Holds a receiver connected with a remote `EmbeddedPermissionControlClient`
  // in browser process, allowing this element to receive PEPC events from
  // browser process.
  HeapMojoReceiver<mojom::blink::EmbeddedPermissionControlClient,
                   HTMLPermissionElement>
      embedded_permission_control_receiver_;

  //  Map holds all current permission statuses, keyed by permission name.
  using PermissionStatusMap =
      HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>;
  PermissionStatusMap permission_status_map_;

  AtomicString type_;

  // Holds reasons for which clicking is currently disabled (if any). Each
  // entry will have an expiration time associated with it, which can be
  // |base::TimeTicks::Max()| if it's indefinite.
  HashMap<DisableReason, base::TimeTicks> clicking_disabled_reasons_;

  Member<HTMLSpanElement> permission_text_span_;
  Member<IntersectionObserver> intersection_observer_;

  // Set to true only if all the corresponding permissions (from `type`
  // attribute) are granted.
  bool permissions_granted_ = false;

  // Set to true only if this element is fully visible on the viewport (observed
  // by IntersectionObserver).
  bool is_fully_visible_ = true;

  // The intersection rectangle between the layout box of this element and the
  // viewport.
  gfx::Rect intersection_rect_;

  // The permission descriptors that correspond to a request made from this
  // permission element. Only computed once, when the `type` attribute is set.
  Vector<mojom::blink::PermissionDescriptorPtr> permission_descriptors_;

  // A bool that tracks whether a specific console message was sent already to
  // ensure it's not sent again.
  bool length_console_error_sent_ = false;
};

// The custom type casting is required for the PermissionElement OT because the
// generated helpers code can lead to a compilation error or an
// HTMLPermissionElement appearing in a document that does not have the
// PermissionElement origin trial enabled (this would result in the creation of
// an HTMLUnknownElement with the "Permission" tag name).
// TODO((crbug.com/339781931): Once the origin trial has ended, these custom
// type casts will no longer be necessary.
template <>
struct DowncastTraits<HTMLPermissionElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLPermissionElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node)) {
      return html_element->IsHTMLPermissionElement();
    }
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element)) {
      return html_element->IsHTMLPermissionElement();
    }
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
