// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/cached_permission_status.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_permission_icon_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Page;
class V8PermissionState;

// For more information, see the explainer here:
// https://github.com/WICG/PEPC/blob/main/explainer.md
// and the design doc here: docs/permissions/pepc.md.
class CORE_EXPORT HTMLPermissionElement
    : public HTMLElement,
      public mojom::blink::EmbeddedPermissionControlClient,
      public LocalFrameView::LifecycleNotificationObserver,
      public CachedPermissionStatus::Client {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool isTypeSupported(const AtomicString& type);

  explicit HTMLPermissionElement(Document&,
                                 std::optional<QualifiedName> = std::nullopt);

  ~HTMLPermissionElement() override;

  const AtomicString& GetType() const;
  String invalidReason() const;
  bool isValid() const;
  V8PermissionState initialPermissionStatus() const;
  V8PermissionState permissionStatus() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(promptaction, kPromptaction)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(promptdismiss, kPromptdismiss)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(validationstatuschange,
                                  kValidationstatuschange)

  void Trace(Visitor*) const override;

  using PermissionStatusMap =
      HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>;
  // CachedPermissionStatus::Client overrides.
  void OnPermissionStatusInitialized(
      PermissionStatusMap initilized_map) override;
  void OnPermissionStatusChange(mojom::blink::PermissionName permission_name,
                                mojom::blink::PermissionStatus status) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void AttachLayoutTree(AttachContext& context) override;
  void DetachLayoutTree(bool performing_reattach) override;
  void Focus(const FocusParams& params) override;
  FocusableState SupportsFocus(UpdateBehavior) const override;
  int DefaultTabIndex() const override;
  CascadeFilter GetCascadeFilter() const override;
  bool CanGeneratePseudoElement(PseudoId) const override;

  bool IsRenderered() const;
  bool granted() const { return PermissionsGranted(); }

  // Given an input type, return permissions list. This method is for testing
  // only.
  static Vector<mojom::blink::PermissionDescriptorPtr>
  ParsePermissionDescriptorsForTesting(const AtomicString& type);

  const Member<HTMLSpanElement>& permission_text_span_for_testing() const {
    return permission_text_span_;
  }

  // Verify whether the element has been registered in browser process.
  bool is_registered_in_browser_process_for_testing() const {
    return is_registered_in_browser_process_;
  }

  // HTMLElement overrides.
  bool IsHTMLPermissionElement() const final { return true; }

 protected:
  // blink::HTMLElement:
  void AttributeChanged(const AttributeModificationParams& params) override;

  // blink::Node:
  void DefaultEventHandler(Event&) override;

  void HandleActivation(Event&, base::OnceClosure on_success);

  bool PermissionsGranted() const {
    return aggregated_permission_status_.has_value() &&
           aggregated_permission_status_ ==
               mojom::blink::PermissionStatus::GRANTED;
  }

  void setType(const AtomicString& type);
  uint16_t GetTranslatedMessageID(uint16_t message_id,
                                  const AtomicString& language_string);
  virtual void UpdateAppearance();

  void UpdateIcon(mojom::blink::PermissionName permission);

  // Update permission statuses and appearance based on the current statuses.
  virtual void UpdatePermissionStatusAndAppearance();

  virtual mojom::blink::EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptor();

  // Called when the |permission_status_map_| is updated to
  // - Ensure that |aggregated_permission_status_| and
  //   |initial_aggregated_permission_status_| are updated.
  void UpdatePermissionStatus();

  HTMLSpanElement* permission_text_span() const {
    return permission_text_span_.Get();
  }

  void SetPreciseLocation(bool);

  bool is_precise_location() const { return is_precise_location_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  virtual Vector<mojom::blink::PermissionDescriptorPtr> ParseType(
      const AtomicString& type);

  bool HasPendingPermissionRequest() const {
    return pending_request_created_.has_value();
  }

 private:
  // TODO(crbug.com/1315595): remove this friend class once migration
  // to blink_unittests_v2 completes.
  friend class DeferredChecker;
  friend class HTMLPermissionElementIntersectionTest;
  friend class HTMLPermissionElementLayoutChangeTest;
  friend class HTMLGeolocationElementIntersectionTest;
  friend class HTMLInstallElementTestBase;
  friend class HTMLPermissionElementTestBase;

  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTestBase, GetTypeAttribute);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationUsingLocationAppearance);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationWatchPositionAppearance);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationTranslateInnerText);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationSetInnerTextAfterRegistration);
  FRIEND_TEST_ALL_PREFIXES(
      HTMLGeolocationElementTest,
      GeolocationPreciseLocationAttributeDoesNotChangeText);
  FRIEND_TEST_ALL_PREFIXES(
      HTMLGeolocationElementTest,
      GeolocationPreciseLocationAttributeCamelCaseDoesNotChangeText);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest, GeolocationAccuracyMode);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           GeolocationAccuracyModeCaseInsensitive);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest, GeolocationStatusChange);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementTest,
                           PermissionStatusChangeAfterDecided);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementSimTest,
                           GeolocationInitializeGrantedText);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementSimTest,
                           InvalidDisplayStyleElement);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementSimTest,
                           BadContrastDisablesElement);
  FRIEND_TEST_ALL_PREFIXES(HTMLGeolocationElementIntersectionTest,
                           IntersectionChanged);
  FRIEND_TEST_ALL_PREFIXES(HTMLInstallElementTestBase, RenderedText);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementClickingEnabledTest,
                           UnclickableBeforeRegistered);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementIntersectionTest,
                           IntersectionChanged);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementIntersectionTest,
                           IntersectionChangedDisableEnableDisable);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementIntersectionTest,
                           ContainerDivRotates);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementIntersectionTest,
                           ContainerDivOpacity);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementIntersectionTest,
                           ContainerDivClipPath);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementIntersectionTest,
                           IntersectionOccluderLogging);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementIntersectionTest,
                           IntersectionVisibleOverlapsRecentAttachedInterval);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementFencedFrameTest,
                           NotAllowedInFencedFrame);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementSimTest,
                           BlockedByMissingFrameAncestorsCSP);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementSimTest,
                           EnableClickingAfterDelay);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementSimTest,
                           FontSizeCanDisableElement);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementSimTest,
                           MovePEPCToAnotherDocument);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementSimTest,
                           RegisterAfterBeingVisible);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementLayoutChangeTest,
                           InvalidatePEPCAfterMove);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementLayoutChangeTest,
                           InvalidatePEPCAfterResize);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementLayoutChangeTest,
                           InvalidatePEPCAfterMoveContainer);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementLayoutChangeTest,
                           InvalidatePEPCAfterTransformContainer);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementLayoutChangeTest,
                           InvalidatePEPCLayoutInAnimationFrameCallback);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementDispatchValidationEventTest,
                           ChangeReasonRestartTimer);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementDispatchValidationEventTest,
                           DisableEnableClicking);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementDispatchValidationEventTest,
                           DisableEnableClickingDifferentReasons);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementTestBase,
                           SetPreciseLocationAttribute);
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementTest, SetTypeAfterInsertedInto);

  enum class DisableReason {
    kUnknown,

    // This element is temporarily disabled for a short period
    // (`kDefaultDisableTimeout`) after being attached to the layout tree.
    kRecentlyAttachedToLayoutTree,

    // This element is disabled because it is outside the bounds of the
    // viewport, or the element is clipped.
    kIntersectionVisibilityOutOfViewPortOrClipped,

    // This element is disabled because because it is covered by other element,
    // or has distorted visual effect.
    kIntersectionVisibilityOccludedOrDistorted,

    // This element is temporarily disabled for a short period
    // (`kDefaultDisableTimeout`) after its intersection rect with the viewport
    // has been changed.
    kIntersectionWithViewportChanged,

    // This element is disabled because of the element's style.
    kInvalidStyle,

    // The element's attribute changed.
    kAttributeChanged,
  };

  // These values are used for histograms. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(UserInteractionDeniedReason)
  enum class UserInteractionDeniedReason {
    kInvalidType = 0,
    kFailedOrHasNotBeenRegistered = 1,
    kRecentlyAttachedToLayoutTree = 2,
    // kIntersectionRecentlyFullyVisible = 3,    Deprecated.
    kInvalidStyle = 4,
    kUntrustedEvent = 5,
    kIntersectionWithViewportChanged = 6,
    kIntersectionVisibilityOutOfViewPortOrClipped = 7,
    kIntersectionVisibilityOccludedOrDistorted = 8,
    kAttributeChanged = 9,
    kMaxValue = kAttributeChanged,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:PermissionElementUserInteractionDeniedReason)

  // These values are used for histograms. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(InvalidStyleReason)
  enum class InvalidStyleReason {
    kNoComputedStyle = 0,
    kNonOpaqueColorOrBackgroundColor = 1,
    kLowConstrastColorAndBackgroundColor = 1,
    kTooSmallFontSize = 3,
    kTooLargeFontSize = 4,
    kInvalidDisplayProperty = 5,

    kMaxValue = kInvalidDisplayProperty,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:PermissionElementInvalidStyleReason)

  // Define the different states of visibility depending on IntersectionObserver
  // tells us why an element is visible/invisible.
  enum class IntersectionVisibility {
    // The element is not fully visible on the screen because it is outside the
    // bounds of the viewport, or the element is clipped.
    kOutOfViewportOrClipped,
    // The element is not fully visible on the screen because it is covered by
    // other element, or has distorted visual effect.
    kOccludedOrDistorted,
    // The element is fully visible on the screen.
    kFullyVisible
  };

  // Customized HeapTaskRunnerTimer class to contain disable reason, firing to
  // indicate that the disable reason is expired.
  class DisableReasonExpireTimer final : public TimerBase {
    DISALLOW_NEW();

   public:
    using TimerFiredFunction = void (HTMLPermissionElement::*)(TimerBase*);

    DisableReasonExpireTimer(HTMLPermissionElement* element,
                             TimerFiredFunction function)
        : TimerBase(element->GetTaskRunner()),
          element_(element),
          function_(function) {}

    ~DisableReasonExpireTimer() final = default;

    void Trace(Visitor* visitor) const { visitor->Trace(element_); }

    void StartOrRestartWithReason(DisableReason reason,
                                  base::TimeDelta interval) {
      if (IsActive()) {
        Stop();
      }

      reason_ = reason;
      StartOneShot(interval, FROM_HERE);
    }

    void set_reason(DisableReason reason) { reason_ = reason; }
    DisableReason reason() const { return reason_; }

   protected:
    void Fired() final { (element_->*function_)(this); }

    base::OnceClosure BindTimerClosure() final {
      return BindOnce(&DisableReasonExpireTimer::RunInternalTrampoline,
                      Unretained(this), WrapWeakPersistent(element_.Get()));
    }

   private:
    // Trampoline used for garbage-collected timer version also checks whether
    // the object has been deemed as dead by the GC but not yet reclaimed. Dead
    // objects that have not been reclaimed yet must not be touched (which is
    // enforced by ASAN poisoning).
    static void RunInternalTrampoline(DisableReasonExpireTimer* timer,
                                      HTMLPermissionElement* element) {
      // If the element have been garbage collected, the timer does not fire.
      if (element) {
        timer->RunInternal();
      }
    }

    WeakMember<HTMLPermissionElement> element_;
    TimerFiredFunction function_;
    DisableReason reason_;
  };

  // Translates `DisableReason` into strings, primarily used for logging
  // console messages.
  static String DisableReasonToString(DisableReason reason);

  // Translates `DisableReason` into `UserInteractionDeniedReason`, primarily
  // used for metrics.
  static UserInteractionDeniedReason DisableReasonToUserInteractionDeniedReason(
      DisableReason reason);

  // Translates `DisableReason` into strings, which will be represented in
  // `invalidReason` attr.
  static AtomicString DisableReasonToInvalidReasonString(DisableReason reason);

  // Ensure there is a connection to the permission service and return it.
  mojom::blink::PermissionService* GetPermissionService();
  void OnPermissionServiceConnectionFailed();

  // Register the permission element, which will trigger an IPC registration
  // call from `permission_service_`.
  // Return false if this element is not allowed to call registration,
  // otherwise, return true and might trigger registration IPC call to browser
  // process.
  bool MaybeRegisterPageEmbeddedPermissionControl();

  // Ensure we reset the PEPC IPC endpoint.
  void EnsureUnregisterPageEmbeddedPermissionControl();

  // blink::Element implements
  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  void AdjustStyle(ComputedStyleBuilder& builder) override;
  void DidRecalcStyle(const StyleRecalcChange change) override;
  void LangAttributeChanged() override;

  // Trigger permissions requesting in browser side by calling mojo
  // PermissionService's API.
  void RequestPageEmbededPermissions();

  // mojom::blink::EmbeddedPermissionControlClient override.
  void OnEmbeddedPermissionControlRegistered(
      bool allowed,
      const std::optional<Vector<mojom::blink::PermissionStatus>>& statuses)
      override;

  // Callback triggered when permission is decided from browser side.
  void OnEmbeddedPermissionsDecided(
      mojom::blink::EmbeddedPermissionControlResult result);

  // Callback triggered when the `disable_reason_expire_timer_` fires.
  void DisableReasonExpireTimerFired(TimerBase*);

  void StopTimerDueToIndefiniteReason(DisableReason reason) {
    if (disable_reason_expire_timer_.IsActive() &&
        disable_reason_expire_timer_.reason() == reason) {
      disable_reason_expire_timer_.Stop();
    }
    MaybeDispatchValidationChangeEvent();
  }

  // Dispatch validation status change event if necessary.
  void MaybeDispatchValidationChangeEvent();

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

  struct ClickingEnabledState {
    // Indicates if the element is clickable.
    bool is_valid;
    // Carries the reason if the element is unclickable, and will be the empty
    // string if |is_valid| is true.
    AtomicString invalid_reason;

    bool operator==(const ClickingEnabledState& other) const {
      return is_valid == other.is_valid &&
             invalid_reason == other.invalid_reason;
    }
  };

  ClickingEnabledState GetClickingEnabledState() const;

  // Refresh disable reasons to remove expired reasons, and update the
  // `disable_reason_expire_timer_` interval. The logic here:
  // - If there's an "indefinitely disabling" for any reason, stop the timer.
  // - Otherwise, keep looping to check if there's a disabling reason that will
  //   be exprired later than the current timer, update the timer based on that
  //   reason. As the result, the timer will always match with the "longest
  //   alive temporary disabling reason".
  void RefreshDisableReasonsAndUpdateTimer();

  void AddConsoleError(String error);
  void AddConsoleWarning(String warning);

  void OnIntersectionChanged(
      const HeapVector<Member<IntersectionObserverEntry>>& entries);

  bool IsStyleValid();
  bool IsMaskedByAncestor() const;

  // A wrapper method which keeps track of logging console messages before
  // calling the HTMLPermissionElementUtils::AdjustedBoundedLength method.
  Length AdjustedBoundedLengthWrapper(const Length& length,
                                      std::optional<float> lower_bound,
                                      std::optional<float> upper_bound,
                                      bool should_multiply_by_content_size);

  // A method which bounds the specified radius on the width and height sides
  // using the provided percentage bounds.
  LengthSize AdjustedPercentBoundedRadius(const LengthSize& length_size,
                                          float width_percent_bound,
                                          float height_percent_bound);

  // Computes the intersection rect of the element with the viewport.
  gfx::Rect ComputeIntersectionRectWithViewport(const Page* page);

  // When the element is first attached to layout and rendered on the page,
  // there would be events causing an extra unresponsive delay that we don't
  // want, for example: the IntersectionObserver will trigger a series of events
  // from invisible to visible, with delays. We will throttle the cooldown
  // time of the events to match the recently_attached cooldown time.
  std::optional<base::TimeDelta> GetRecentlyAttachedTimeoutRemaining() const;

  // When the element's type is invalid it enters "fallback" mode where it
  // starts behaving more or less like a HTMLUnknownElement. Child nodes are no
  // longer hidden and it no longer handles DOMActivation events to trigger
  // permission requests. Once fallback mode is entered the element does not
  // revert back.
  void EnableFallbackMode();

  // Report an issue to the devtools issues panel, specifically related to the
  // permission element's activation being disabled.
  void ReportActivationDisabledAuditsIssue(DisableReason reason);

  IntersectionVisibility IntersectionVisibilityForTesting() const {
    return intersection_visibility_;
  }

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;

  // Holds a receiver connected with a remote `EmbeddedPermissionControlClient`
  // in browser process, allowing this element to receive PEPC events from
  // browser process.
  HeapMojoReceiver<mojom::blink::EmbeddedPermissionControlClient,
                   HTMLPermissionElement>
      embedded_permission_control_receiver_;

  // Map holds all current permission statuses, keyed by permission name.
  PermissionStatusMap permission_status_map_;

  // Hold the first-received permission status in this object's lifetime and the
  // current permission status. If this object is a grouped permission element,
  // it aggregates the statuses by taking the most restrictive status.
  std::optional<mojom::blink::PermissionStatus>
      initial_aggregated_permission_status_;
  std::optional<mojom::blink::PermissionStatus> aggregated_permission_status_;

  AtomicString type_;

  bool is_precise_location_ = false;

  bool is_registered_in_browser_process_ = false;

  bool is_cache_registered_ = false;

  // Holds reasons for which clicking is currently disabled (if any). Each
  // entry will have an expiration time associated with it, which can be
  // |base::TimeTicks::Max()| if it's indefinite.
  HashMap<DisableReason, base::TimeTicks> clicking_disabled_reasons_;

  // A element which contains the internal permission elements(text and icon).
  Member<HTMLDivElement> permission_container_;
  Member<HTMLSpanElement> permission_text_span_;
  Member<HTMLPermissionIconElement> permission_internal_icon_;
  Member<IntersectionObserver> intersection_observer_;

  // Keeps track of the time a request was created.
  std::optional<base::TimeTicks> pending_request_created_;

  // Observed by IntersectionObserver to indicate the fully visibility state of
  // the element on the viewport.
  IntersectionVisibility intersection_visibility_ =
      IntersectionVisibility::kFullyVisible;

  // Track the node which is overlapping this element.
  DOMNodeId occluder_node_id_ = kInvalidDOMNodeId;

  // Store the up-to-date click state.
  ClickingEnabledState clicking_enabled_state_{false, AtomicString()};

  // The intersection rectangle between the layout box of this element and the
  // viewport.
  std::optional<gfx::Rect> intersection_rect_;

  // The permission descriptors that correspond to a request made from this
  // permission element. Only computed once, when the `type` attribute is set.
  Vector<mojom::blink::PermissionDescriptorPtr> permission_descriptors_;

  // A bool that tracks whether a specific console message was sent already to
  // ensure it's not sent again.
  bool length_console_error_sent_ = false;

  // The timer firing to indicate the temporary disable reason is expired. The
  // fire interval time should match the maximum timetick (not
  // base::TimeTicks::Max()), which is the timetick of the longest alive
  // temporary disabling reason in `clicking_disabled_reasons_`.
  DisableReasonExpireTimer disable_reason_expire_timer_;

  // Whether the elements has entered fallback mode. See |EnableFallbackMode|.
  bool fallback_mode_ = false;
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
