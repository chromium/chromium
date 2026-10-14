// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

class HTMLMenuListElement;
class HTMLMenuOwnerElement;

class CORE_EXPORT HTMLMenuItemElementBase : public HTMLElement {
 public:
  HTMLMenuItemElementBase(const QualifiedName& tag_name, Document&);
  ~HTMLMenuItemElementBase() override;
  void Trace(Visitor* visitor) const override;

  HTMLMenuOwnerElement* OwningMenuElement() const {
    return owning_menu_element_;
  }

  HTMLMenuOwnerElement* OwnerElementForList() const {
    return owning_menu_element_;
  }

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  bool IsDisabledFormControl() const override;
  bool IsKeyboardFocusableSlow(
      UpdateBehavior update_behavior =
          UpdateBehavior::kStyleAndLayout) const override;

  bool HasActivationBehavior() const override;
  void RunActivationBehavior(Event&, EventDispatchHandlingState*) override;
  void DefaultEventHandler(Event&) override;
  static bool IsActivationFromKeyboard(const Event* activate_event);

  bool MatchesEnabledPseudoClass() const override;

  void ParseAttribute(const AttributeModificationParams&) override;
  bool ShouldHaveFocusAppearance() const override;

  // Returns true if this menuitem is scrolled into view within its ancestor
  // menu owner element.
  bool IsVisibleInViewport();

 protected:
  FocusableState SupportsFocus(UpdateBehavior update_behavior) const override;

  enum class ActivationKeyboardEventType {
    kNotKeyboard,
    kUntrustedKeyboard,
    kTrustedKeyboard,
  };
  static ActivationKeyboardEventType GetActivationKeyboardEventType(
      const Event& activation_event);

  virtual void ActivateMenuItem(ActivationKeyboardEventType) = 0;
  Element* CloseOutermostContainingMenuList();
  HTMLMenuListElement* FindOutermostContainingMenuList();
  bool HasOwnerMenuList() const;

  Member<HTMLMenuOwnerElement> owning_menu_element_;

 private:
  void HandleMenuPointerEvents(Event&);
  void HandleMenuKeyboardEvents(Event&);
  void ResetAncestorElementCache();
};

class CORE_EXPORT HTMLMenuItemCheckableElementBase
    : public HTMLMenuItemElementBase {
 public:
  enum class CheckedBehavior {
    kFireEvents,
    kNoEvents,
  };

  HTMLMenuItemCheckableElementBase(const QualifiedName& tag_name, Document&);
  ~HTMLMenuItemCheckableElementBase() override;

  bool checked() const { return is_checked_; }
  bool checkedForBinding() const { return checked(); }
  void setCheckedForBinding(bool checked) {
    setChecked(checked, CheckedBehavior::kNoEvents);
  }
  bool ShouldAppearChecked() const { return checked(); }

  void setChecked(bool checked, CheckedBehavior event_behavior);

  bool MatchesDefaultPseudoClass() const override;
  void ParseAttribute(const AttributeModificationParams&) override;
  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;

 protected:
  void ActivateMenuItem(ActivationKeyboardEventType) override;
  virtual void UpdateForExclusivity() {}
  virtual bool ClosesMenuListOnActivation() const = 0;

 private:
  bool is_checked_ = false;
  bool is_default_checkedness_overridden_ = false;
};

template <>
struct DowncastTraits<HTMLMenuItemElementBase> {
  static bool AllowFrom(const Node& node) {
    const HTMLElement* html_element = DynamicTo<HTMLElement>(node);
    return html_element && AllowFrom(*html_element);
  }
  static bool AllowFrom(const HTMLElement& html_element) {
    return html_element.HasTagName(html_names::kMenuitemTag) ||
           html_element.HasTagName(html_names::kMenuitemcheckboxTag) ||
           html_element.HasTagName(html_names::kMenuitemradioTag);
  }
};

template <>
struct DowncastTraits<HTMLMenuItemCheckableElementBase> {
  static bool AllowFrom(const Node& node) {
    const HTMLElement* html_element = DynamicTo<HTMLElement>(node);
    return html_element && AllowFrom(*html_element);
  }
  static bool AllowFrom(const HTMLElement& html_element) {
    return html_element.HasTagName(html_names::kMenuitemcheckboxTag) ||
           html_element.HasTagName(html_names::kMenuitemradioTag);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_ITEM_ELEMENT_BASE_H_
