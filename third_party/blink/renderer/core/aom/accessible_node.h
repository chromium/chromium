// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class AccessibleNodeList;
class AXObjectCache;
class Document;
class Element;
class QualifiedName;

// All of the properties of AccessibleNode that have type "string".
enum class AOMStringProperty {
  kAutocomplete,
  kAriaBrailleLabel,
  kAriaBrailleRoleDescription,
  kChecked,
  kColIndexText,
  kCurrent,
  kDescription,
  kHasPopup,
  kInvalid,
  kKeyShortcuts,
  kLabel,
  kLive,
  kOrientation,
  kPlaceholder,
  kPressed,
  kRelevant,
  kRole,
  kRoleDescription,
  kRowIndexText,
  kSort,
  kValueText,
  kVirtualContent
};

// All of the properties of AccessibleNode that have type "boolean".
enum class AOMBooleanProperty {
  kAtomic,
  kBusy,
  kDisabled,
  kExpanded,
  kHidden,
  kModal,
  kMultiline,
  kMultiselectable,
  kReadOnly,
  kRequired,
  kSelected
};

// All of the properties of AccessibleNode that have an unsigned integer type.
enum class AOMUIntProperty {
  kColIndex,
  kColSpan,
  kLevel,
  kPosInSet,
  kRowIndex,
  kRowSpan,
};

enum class AOMRelationProperty {
  kActiveDescendant,
};

enum class AOMRelationListProperty {
  kDescribedBy,
  kDetails,
  kControls,
  kErrorMessage,
  kFlowTo,
  kLabeledBy,
  kOwns,
};

// All of the properties of AccessibleNode that have a signed integer type.
// (These all allow the value -1.)
enum class AOMIntProperty { kColCount, kRowCount, kSetSize };

// All of the properties of AccessibleNode that have a floating-point type.
enum class AOMFloatProperty { kValueMax, kValueMin, kValueNow };

class AccessibleNode;

class CORE_EXPORT AOMPropertyClient {
 public:
  virtual void AddStringProperty(AOMStringProperty, const String&) = 0;
  virtual void AddBooleanProperty(AOMBooleanProperty, bool) = 0;
  virtual void AddFloatProperty(AOMFloatProperty, float) = 0;
  virtual void AddRelationProperty(AOMRelationProperty,
                                   const AccessibleNode&) = 0;
  virtual void AddRelationListProperty(AOMRelationListProperty,
                                       const AccessibleNodeList&) = 0;
};

// Accessibility Object Model node
// Explainer: https://github.com/WICG/aom/blob/gh-pages/explainer.md
// Spec: https://wicg.github.io/aom/spec/
class CORE_EXPORT AccessibleNode : public EventTarget,
                                   public ElementRareDataField {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AccessibleNode(Document&);
  explicit AccessibleNode(Element*);
  ~AccessibleNode() override;

  static AccessibleNode* Create(Document&);

  // Gets the associated element, if any.
  Element* element() const { return element_.Get(); }

  // Gets the associated document.
  Document* GetDocument() const;

  // Returns the parent of this node.
  AccessibleNode* GetParent() { return parent_.Get(); }

  // Children. These are only virtual AccessibleNodes that were added
  // explicitly, never AccessibleNodes from DOM Elements.
  HeapVector<Member<AccessibleNode>> GetChildren() { return children_; }

  // Returns the given string property.
  const AtomicString& GetProperty(AOMStringProperty) const;

  // Returns the given relation property if the Element has an AccessibleNode.
  static AccessibleNode* GetProperty(Element*, AOMRelationProperty);

  // Returns the given relation list property if the Element has an
  // AccessibleNode.
  static AccessibleNodeList* GetProperty(Element*, AOMRelationListProperty);
  static bool GetProperty(Element*,
                          AOMRelationListProperty,
                          HeapVector<Member<Element>>&);

  // Returns the given boolean property.
  std::optional<bool> GetProperty(AOMBooleanProperty) const;

  // Returns the value of the given property if the
  // Element has an AccessibleNode. Sets |isNull| if the property and
  // attribute are not present.
  static std::optional<int32_t> GetProperty(Element*, AOMIntProperty);
  static std::optional<uint32_t> GetProperty(Element*, AOMUIntProperty);
  static std::optional<float> GetProperty(Element*, AOMFloatProperty);

  // Does the attribute value match one of the ARIA undefined patterns for
  // boolean and token properties?
  // These include the empty string ("") or "undefined" as a literal.
  // See ARIA 1.1 Sections 6.2 and 6.3, as well as properties that specifically
  // indicate a supported value of "undefined".
  static bool IsUndefinedAttrValue(const AtomicString&);

  // Returns the value of the given string property if the
  // Element has an AccessibleNode, otherwise returns the equivalent
  // ARIA attribute.
  static const AtomicString& GetPropertyOrARIAAttribute(Element*,
                                                        AOMStringProperty);

  static const AtomicString& GetPropertyOrARIAAttributeValue(
      Element* element,
      AOMRelationProperty property);

  // Returns the given relation property if the Element has an AccessibleNode,
  // otherwise returns the equivalent ARIA attribute.
  static Element* GetPropertyOrARIAAttribute(Element*, AOMRelationProperty);

  // Returns true and provides the the value of the given relation
  // list property if the Element has an AccessibleNode, or if it has
  // the equivalent ARIA attribute. Otherwise returns false.
  static bool GetPropertyOrARIAAttribute(Element*,
                                         AOMRelationListProperty,
                                         HeapVector<Member<Element>>&);

  // Returns the value of the given property if the
  // Element has an AccessibleNode, otherwise returns the equivalent
  // ARIA attribute. Sets |isNull| if the property and attribute are not
  // present.
  static bool GetPropertyOrARIAAttribute(Element*,
                                         AOMBooleanProperty,
                                         bool& is_null);
  static float GetPropertyOrARIAAttribute(Element*,
                                          AOMFloatProperty,
                                          bool& is_null);
  static int32_t GetPropertyOrARIAAttribute(Element*,
                                            AOMIntProperty,
                                            bool& is_null);
  static uint32_t GetPropertyOrARIAAttribute(Element*,
                                             AOMUIntProperty,
                                             bool& is_null);

  // Iterates over all AOM properties. For each one, calls AOMPropertyClient
  // with the value of the AOM property if set.
  void GetAllAOMProperties(AOMPropertyClient*);

  AccessibleNode* activeDescendant() const;
  void setActiveDescendant(AccessibleNode*);

  std::optional<bool> atomic() const;
  void setAtomic(std::optional<bool>);

  AtomicString autocomplete() const;
  void setAutocomplete(const AtomicString&);

  std::optional<bool> busy() const;
  void setBusy(std::optional<bool>);

  AtomicString brailleLabel() const;
  void setBrailleLabel(const AtomicString&);

  AtomicString brailleRoleDescription() const;
  void setBrailleRoleDescription(const AtomicString&);

  AtomicString checked() const;
  void setChecked(const AtomicString&);

  std::optional<int32_t> colCount() const;
  void setColCount(std::optional<int32_t>);

  std::optional<uint32_t> colIndex() const;
  void setColIndex(std::optional<uint32_t>);

  AtomicString colIndexText() const;
  void setColIndexText(const AtomicString&);

  std::optional<uint32_t> colSpan() const;
  void setColSpan(std::optional<uint32_t>);

  AccessibleNodeList* controls() const;
  void setControls(AccessibleNodeList*);

  AtomicString current() const;
  void setCurrent(const AtomicString&);

  AccessibleNodeList* describedBy();
  void setDescribedBy(AccessibleNodeList*);

  AtomicString description() const;
  void setDescription(const AtomicString&);

  AccessibleNodeList* details() const;
  void setDetails(AccessibleNodeList*);

  std::optional<bool> disabled() const;
  void setDisabled(std::optional<bool>);

  AccessibleNodeList* errorMessage() const;
  void setErrorMessage(AccessibleNodeList*);

  std::optional<bool> expanded() const;
  void setExpanded(std::optional<bool>);

  AccessibleNodeList* flowTo() const;
  void setFlowTo(AccessibleNodeList*);

  AtomicString hasPopup() const;
  void setHasPopup(const AtomicString&);

  std::optional<bool> hidden() const;
  void setHidden(std::optional<bool>);

  AtomicString invalid() const;
  void setInvalid(const AtomicString&);

  AtomicString keyShortcuts() const;
  void setKeyShortcuts(const AtomicString&);

  AtomicString label() const;
  void setLabel(const AtomicString&);

  AccessibleNodeList* labeledBy();
  void setLabeledBy(AccessibleNodeList*);

  std::optional<uint32_t> level() const;
  void setLevel(std::optional<uint32_t>);

  AtomicString live() const;
  void setLive(const AtomicString&);

  std::optional<bool> modal() const;
  void setModal(std::optional<bool>);

  std::optional<bool> multiline() const;
  void setMultiline(std::optional<bool>);

  std::optional<bool> multiselectable() const;
  void setMultiselectable(std::optional<bool>);

  AtomicString orientation() const;
  void setOrientation(const AtomicString&);

  AccessibleNodeList* owns() const;
  void setOwns(AccessibleNodeList*);

  AtomicString placeholder() const;
  void setPlaceholder(const AtomicString&);

  std::optional<uint32_t> posInSet() const;
  void setPosInSet(std::optional<uint32_t>);

  AtomicString pressed() const;
  void setPressed(const AtomicString&);

  std::optional<bool> readOnly() const;
  void setReadOnly(std::optional<bool>);

  AtomicString relevant() const;
  void setRelevant(const AtomicString&);

  std::optional<bool> required() const;
  void setRequired(std::optional<bool>);

  AtomicString role() const;
  void setRole(const AtomicString&);

  AtomicString roleDescription() const;
  void setRoleDescription(const AtomicString&);

  std::optional<int32_t> rowCount() const;
  void setRowCount(std::optional<int32_t>);

  std::optional<uint32_t> rowIndex() const;
  void setRowIndex(std::optional<uint32_t>);

  AtomicString rowIndexText() const;
  void setRowIndexText(const AtomicString&);

  std::optional<uint32_t> rowSpan() const;
  void setRowSpan(std::optional<uint32_t>);

  std::optional<bool> selected() const;
  void setSelected(std::optional<bool>);

  std::optional<int32_t> setSize() const;
  void setSetSize(std::optional<int32_t>);

  AtomicString sort() const;
  void setSort(const AtomicString&);

  std::optional<float> valueMax() const;
  void setValueMax(std::optional<float>);

  std::optional<float> valueMin() const;
  void setValueMin(std::optional<float>);

  std::optional<float> valueNow() const;
  void setValueNow(std::optional<float>);

  AtomicString valueText() const;
  void setValueText(const AtomicString&);

  AtomicString virtualContent() const;
  void setVirtualContent(const AtomicString&);

  AccessibleNodeList* childNodes();

  void appendChild(AccessibleNode*, ExceptionState&);
  void removeChild(AccessibleNode*, ExceptionState&);

  // Called when an accessible node is removed from document.
  void DetachedFromDocument();
  Document* GetAncestorDocument();

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(accessibleclick, kAccessibleclick)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(accessiblecontextmenu, kAccessiblecontextmenu)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(accessibledecrement, kAccessibledecrement)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(accessiblefocus, kAccessiblefocus)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(accessibleincrement, kAccessibleincrement)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(accessiblescrollintoview,
                                  kAccessiblescrollintoview)

  void Trace(Visitor*) const override;

 protected:
  friend class AccessibleNodeList;
  void OnRelationListChanged(AOMRelationListProperty);

 private:
  static bool IsStringTokenProperty(AOMStringProperty);
  static const AtomicString& GetElementOrInternalsARIAAttribute(
      Element& element,
      const QualifiedName& attribute,
      bool is_token_attr = false);
  void SetStringProperty(AOMStringProperty, const AtomicString&);
  void SetRelationProperty(AOMRelationProperty, AccessibleNode*);
  void SetRelationListProperty(AOMRelationListProperty, AccessibleNodeList*);
  void SetBooleanProperty(AOMBooleanProperty, std::optional<bool> value);
  void SetIntProperty(AOMIntProperty, std::optional<int32_t> value);
  void SetUIntProperty(AOMUIntProperty, std::optional<uint32_t> value);
  void SetFloatProperty(AOMFloatProperty, std::optional<float> value);
  void NotifyAttributeChanged(const blink::QualifiedName&);
  AXObjectCache* GetAXObjectCache();

  Vector<std::pair<AOMStringProperty, AtomicString>> string_properties_;
  Vector<std::pair<AOMBooleanProperty, bool>> boolean_properties_;
  Vector<std::pair<AOMFloatProperty, float>> float_properties_;
  Vector<std::pair<AOMIntProperty, int32_t>> int_properties_;
  Vector<std::pair<AOMUIntProperty, uint32_t>> uint_properties_;
  HeapVector<std::pair<AOMRelationProperty, Member<AccessibleNode>>>
      relation_properties_;
  HeapVector<std::pair<AOMRelationListProperty, Member<AccessibleNodeList>>>
      relation_list_properties_;

  // This object's owner Element, if it corresponds to an Element.
  const Member<Element> element_;

  // The object's owner Document. Only set if |element_| is nullptr.
  Member<Document> document_;

  // This object's AccessibleNode children, which must be only virtual
  // AccessibleNodes (with no associated Element).
  HeapVector<Member<AccessibleNode>> children_;

  // This object's AccessibleNode parent. Only set if this is a
  // virtual AccessibleNode that's in the tree.
  Member<AccessibleNode> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_H_
