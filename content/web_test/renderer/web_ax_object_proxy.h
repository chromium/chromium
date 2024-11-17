// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_AX_OBJECT_PROXY_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_AX_OBJECT_PROXY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_event_intent.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
}

namespace content {

class WebAXObjectProxy : public gin::Wrappable<WebAXObjectProxy> {
 public:
  class Factory {
   public:
    virtual ~Factory() {}
    virtual v8::Local<v8::Object> GetOrCreate(
        const blink::WebAXObject& object) = 0;
    virtual blink::WebAXContext* GetAXContext() = 0;
  };

  static gin::WrapperInfo kWrapperInfo;

  WebAXObjectProxy(const blink::WebAXObject& object, Factory* factory);

  WebAXObjectProxy(const WebAXObjectProxy&) = delete;
  WebAXObjectProxy& operator=(const WebAXObjectProxy&) = delete;

  ~WebAXObjectProxy() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  virtual v8::Local<v8::Object> GetChildAtIndex(unsigned index);
  virtual bool IsRoot() const;
  bool IsEqualToObject(const blink::WebAXObject& object);

  void NotificationReceived(
      blink::WebLocalFrame* frame,
      const std::string& notification_name,
      const std::vector<ui::AXEventIntent>& event_intents);
  void Reset();

  const blink::WebAXObject& accessibility_object() const {
    return accessibility_object_;
  }

 protected:
  Factory* factory() const { return factory_; }

  bool IsDetached() const { return !factory_ || !factory_->GetAXContext(); }

 private:
  friend class WebAXObjectProxyBindings;

  // Returns true if successful.
  bool UpdateLayout();
  ui::AXNodeData GetAXNodeData() const;

  // Bound properties.
  std::string Role();
  std::string StringValue();
  std::string Language();
  int X();
  int Y();
  int Width();
  int Height();
  v8::Local<v8::Value> InPageLinkTarget(v8::Isolate* isolate);
  int IntValue();
  int MinValue();
  int MaxValue();
  int StepValue();
  std::string ValueDescription();
  int ChildrenCount();

  // The following selection functions return global information about the
  // current selection and can be called on any object in the tree.
  bool SelectionIsBackward();
  v8::Local<v8::Value> SelectionAnchorObject(v8::Isolate* isolate);
  int SelectionAnchorOffset();
  std::string SelectionAnchorAffinity();
  v8::Local<v8::Value> SelectionFocusObject(v8::Isolate* isolate);
  int SelectionFocusOffset();
  std::string SelectionFocusAffinity();

  // The following selection functions return text offsets calculated starting
  // at this object. They only report on a selection that is placed on the
  // current object or on any of its descendants.
  // For example, they can be used to retrieve the selection in an input or
  // a textarea.
  int SelectionStart();
  int SelectionEnd();

  bool IsAtomic();
  bool IsAutofillAvailable();
  bool IsBusy();
  bool IsRequired();
  bool IsEditableRoot();
  bool IsEditable();
  bool IsRichlyEditable();
  bool IsFocused();
  bool IsFocusable();
  bool IsModal();
  bool IsSelected();
  bool IsSelectable();
  bool IsMultiLine();
  bool IsMultiSelectable();
  bool IsExpanded();
  std::string Checked();
  bool IsVisible();
  // Exposes the visited state of a link.
  bool IsVisited();
  bool IsCollapsed();
  bool IsValid();
  bool IsReadOnly();
  bool IsIgnored();
  std::string Restriction();
  v8::Local<v8::Object> ActiveDescendant();
  unsigned int BackgroundColor();
  unsigned int Color();
  // For input elements of type color.
  unsigned int ColorValue();
  std::string FontFamily();
  float FontSize();
  std::string Autocomplete();
  std::string Current();
  std::string HasPopup();
  std::string Invalid();
  std::string KeyShortcuts();
  std::string Live();
  std::string Orientation();
  std::string Relevant();
  std::string RoleDescription();
  std::string Sort();
  std::string Url();
  int HierarchicalLevel();
  int PosInSet();
  int SetSize();
  int ClickPointX();
  int ClickPointY();
  int32_t AriaColumnCount();
  uint32_t AriaColumnIndex();
  uint32_t AriaColumnSpan();
  int32_t AriaRowCount();
  uint32_t AriaRowIndex();
  uint32_t AriaRowSpan();
  int32_t RowCount();
  int32_t RowHeadersCount();
  int32_t ColumnCount();
  int32_t ColumnHeadersCount();
  bool IsClickable();
  float BoundsX();
  float BoundsY();
  float BoundsWidth();
  float BoundsHeight();

  // Bound methods.
  v8::Local<v8::Object> AriaActiveDescendantElement();
  v8::Local<v8::Object> AriaControlsElementAtIndex(unsigned index);
  v8::Local<v8::Object> AriaDetailsElementAtIndex(unsigned index);
  v8::Local<v8::Object> AriaErrorMessageElementAtIndex(unsigned index);
  v8::Local<v8::Object> AriaFlowToElementAtIndex(unsigned index);
  v8::Local<v8::Object> AriaOwnsElementAtIndex(unsigned index);
  std::string AllAttributes();
  std::string AttributesOfChildren();
  std::string BoundsForRange(int start, int end);
  v8::Local<v8::Object> ChildAtIndex(int index);
  v8::Local<v8::Object> ElementAtPoint(int x, int y);
  v8::Local<v8::Object> TableHeader();
  v8::Local<v8::Object> RowHeaderAtIndex(unsigned index);
  v8::Local<v8::Object> ColumnHeaderAtIndex(unsigned index);
  std::string RowIndexRange();
  std::string ColumnIndexRange();
  v8::Local<v8::Object> CellForColumnAndRow(int column, int row);
  void SetSelectedTextRange(int selection_start, int length);
  bool SetSelection(v8::Isolate* isolate,
                    v8::Local<v8::Value> anchor_object,
                    int anchor_offset,
                    v8::Local<v8::Value> focus_object,
                    int focus_offset);
  bool IsAttributeSettable(const std::string& attribute);
  bool IsPressActionSupported();
  bool IsIncrementActionSupported();
  bool IsDecrementActionSupported();
  bool HasDefaultAction();
  v8::Local<v8::Object> ParentElement();
  void Increment();
  void Decrement();
  void ShowMenu();
  void Press();
  bool SetValue(const std::string& value);
  bool IsEqual(v8::Isolate* isolate, v8::Local<v8::Object> proxy);
  void SetNotificationListener(v8::Isolate* isolate,
                               v8::Local<v8::Function> callback);
  void UnsetNotificationListener();
  void TakeFocus();
  void ScrollToMakeVisible();
  void ScrollToMakeVisibleWithSubFocus(int x, int y, int width, int height);
  void ScrollToGlobalPoint(int x, int y);
  void ScrollUp();
  void ScrollDown();
  int ScrollX();
  int ScrollY();
  std::string ToString();
  int WordStart(int character_index);
  int WordEnd(int character_index);
  v8::Local<v8::Object> NextOnLine();
  v8::Local<v8::Object> PreviousOnLine();
  std::string MisspellingAtIndex(int index);
  v8::Local<v8::Object> OffsetContainer();
  float BoundsInContainerX();
  float BoundsInContainerY();
  float BoundsInContainerWidth();
  float BoundsInContainerHeight();
  bool HasNonIdentityTransform();

  std::string Name();
  std::string NameFrom();
  int NameElementCount();
  v8::Local<v8::Object> NameElementAtIndex(unsigned index);

  std::string Description();
  std::string DescriptionFrom();
  int MisspellingsCount();
  int DescriptionElementCount();
  v8::Local<v8::Object> DescriptionElementAtIndex(unsigned index);

  std::vector<std::string> GetMisspellings() const;

  std::string Placeholder();

  blink::WebAXObject accessibility_object_;
  raw_ptr<Factory> factory_;

  v8::Global<v8::Function> notification_callback_;
};

class RootWebAXObjectProxy : public WebAXObjectProxy {
 public:
  RootWebAXObjectProxy(const blink::WebAXObject&, Factory*);

  v8::Local<v8::Object> GetChildAtIndex(unsigned index) override;
  bool IsRoot() const override;
};

class WebAXObjectProxyList : public WebAXObjectProxy::Factory {
 public:
  explicit WebAXObjectProxyList(v8::Isolate* isolate, blink::WebAXContext&);
  ~WebAXObjectProxyList() override;

  void Clear();
  void Remove(unsigned axid);
  v8::Local<v8::Object> GetOrCreate(const blink::WebAXObject&) override;
  blink::WebAXContext* GetAXContext() override;

 private:
  raw_ptr<v8::Isolate, DanglingUntriaged> isolate_;
  // Maps from AxID to corresponding v8 wrapper object for an AX object..
  std::unordered_map<unsigned, v8::Global<v8::Object>> ax_objects_;
  const raw_ptr<blink::WebAXContext, DanglingUntriaged> ax_context_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_AX_OBJECT_PROXY_H_
