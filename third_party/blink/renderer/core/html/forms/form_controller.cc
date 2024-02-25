/*
 * Copyright (C) 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/forms/form_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/file_chooser.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

inline HTMLFormElement* OwnerFormForState(const ListedElement& control) {
  // Assume controls with form attribute have no owners because we restore
  // state during parsing and form owners of such controls might be
  // indeterminate.
  return control.ToHTMLElement().FastHasAttribute(html_names::kFormAttr)
             ? nullptr
             : control.Form();
}

const AtomicString& ControlType(const ListedElement& control) {
  if (auto* control_element = DynamicTo<HTMLFormControlElement>(control))
    return control_element->type();
  return To<ElementInternals>(control).Target().localName();
}

bool IsDirtyControl(const ListedElement& control) {
  if (auto* form_control_element =
          DynamicTo<HTMLFormControlElementWithState>(control))
    return form_control_element->UserHasEditedTheField();
  if (control.IsElementInternals()) {
    // We have no ways to know the dirtiness of a form-associated custom
    // element.  Assume it is dirty if it has focus.
    // TODO(tkent): If this approach is not enough, we should check existence
    // of past user-input events such as 'mousedown', 'keydown', 'touchstart'.
    return control.ToHTMLElement().HasFocusWithin();
  }
  DCHECK(!control.ClassSupportsStateRestore());
  return false;
}

}  // namespace

// ----------------------------------------------------------------------------

// Serilized form of FormControlState:
//  (',' means strings around it are separated in stateVector.)
//
// SerializedControlState ::= SkipState | RestoreState
// SkipState ::= '0'
// RestoreState ::= UnsignedNumber, ControlValue+
// UnsignedNumber ::= [0-9]+
// ControlValue ::= arbitrary string
//
// RestoreState has a sequence of ControlValues. The length of the
// sequence is represented by UnsignedNumber.

void FormControlState::SerializeTo(Vector<String>& state_vector) const {
  DCHECK(!IsFailure());
  state_vector.push_back(String::Number(values_.size()));
  for (const auto& value : values_)
    state_vector.push_back(value.IsNull() ? g_empty_string : value);
}

FormControlState FormControlState::Deserialize(
    const Vector<String>& state_vector,
    wtf_size_t& index) {
  if (index >= state_vector.size())
    return FormControlState(kTypeFailure);
  unsigned value_size = state_vector[index++].ToUInt();
  if (!value_size)
    return FormControlState();
  if (index + value_size > state_vector.size())
    return FormControlState(kTypeFailure);
  FormControlState state;
  state.values_.reserve(value_size);
  for (unsigned i = 0; i < value_size; ++i)
    state.Append(state_vector[index++]);
  return state;
}

// ----------------------------------------------------------------------------

class ControlKey {
 public:
  ControlKey(StringImpl* = nullptr, StringImpl* = nullptr);
  ~ControlKey();
  ControlKey(const ControlKey&);
  ControlKey& operator=(const ControlKey&);

  StringImpl* GetName() const { return name_; }
  StringImpl* GetType() const { return type_; }

  // Hash table deleted values, which are only constructed and never copied or
  // destroyed.
  ControlKey(WTF::HashTableDeletedValueType) : name_(HashTableDeletedValue()) {}
  bool IsHashTableDeletedValue() const {
    return name_ == HashTableDeletedValue();
  }

 private:
  void Ref() const;
  void Deref() const;

  static StringImpl* HashTableDeletedValue() {
    return reinterpret_cast<StringImpl*>(-1);
  }

  StringImpl* name_;
  StringImpl* type_;
};

ControlKey::ControlKey(StringImpl* name, StringImpl* type)
    : name_(name), type_(type) {
  Ref();
}

ControlKey::~ControlKey() {
  Deref();
}

ControlKey::ControlKey(const ControlKey& other)
    : name_(other.GetName()), type_(other.GetType()) {
  Ref();
}

ControlKey& ControlKey::operator=(const ControlKey& other) {
  other.Ref();
  Deref();
  name_ = other.GetName();
  type_ = other.GetType();
  return *this;
}

void ControlKey::Ref() const {
  if (GetName())
    GetName()->AddRef();
  if (GetType())
    GetType()->AddRef();
}

void ControlKey::Deref() const {
  if (GetName())
    GetName()->Release();
  if (GetType())
    GetType()->Release();
}

inline bool operator==(const ControlKey& a, const ControlKey& b) {
  return a.GetName() == b.GetName() && a.GetType() == b.GetType();
}

struct ControlKeyHashTraits : SimpleClassHashTraits<ControlKey> {
  static unsigned GetHash(const ControlKey& key) {
    return StringHasher::HashMemory<sizeof(ControlKey)>(&key);
  }
};

// ----------------------------------------------------------------------------

// SavedFormState represents a set of FormControlState.
// It typically manages controls associated to a single <form>.  Controls
// without owner forms are managed by a dedicated SavedFormState.
class SavedFormState {
  USING_FAST_MALLOC(SavedFormState);

 public:
  SavedFormState() : control_state_count_(0) {}
  SavedFormState(const SavedFormState&) = delete;
  SavedFormState& operator=(const SavedFormState&) = delete;

  static std::unique_ptr<SavedFormState> Deserialize(const Vector<String>&,
                                                     wtf_size_t& index);
  void SerializeTo(Vector<String>&) const;
  bool IsEmpty() const { return state_for_new_controls_.empty(); }
  void AppendControlState(const AtomicString& name,
                          const AtomicString& type,
                          const FormControlState&);
  FormControlState TakeControlState(const AtomicString& name,
                                    const AtomicString& type);

  Vector<String> GetReferencedFilePaths() const;

 private:
  using ControlStateMap =
      HashMap<ControlKey, Deque<FormControlState>, ControlKeyHashTraits>;
  ControlStateMap state_for_new_controls_;
  wtf_size_t control_state_count_;
};

static bool IsNotFormControlTypeCharacter(UChar ch) {
  return ch != '-' && (ch > 'z' || ch < 'a');
}

std::unique_ptr<SavedFormState> SavedFormState::Deserialize(
    const Vector<String>& state_vector,
    wtf_size_t& index) {
  if (index >= state_vector.size())
    return nullptr;
  // FIXME: We need String::toSizeT().
  wtf_size_t item_count = state_vector[index++].ToUInt();
  if (!item_count)
    return nullptr;
  std::unique_ptr<SavedFormState> saved_form_state =
      base::WrapUnique(new SavedFormState);
  while (item_count--) {
    if (index + 1 >= state_vector.size())
      return nullptr;
    String name = state_vector[index++];
    String type = state_vector[index++];
    FormControlState state = FormControlState::Deserialize(state_vector, index);
    if (type.empty() ||
        (type.Find(IsNotFormControlTypeCharacter) != kNotFound &&
         !CustomElement::IsValidName(AtomicString(type))) ||
        state.IsFailure())
      return nullptr;
    saved_form_state->AppendControlState(AtomicString(name), AtomicString(type),
                                         state);
  }
  return saved_form_state;
}

void SavedFormState::SerializeTo(Vector<String>& state_vector) const {
  state_vector.push_back(String::Number(control_state_count_));
  for (const auto& form_control : state_for_new_controls_) {
    const ControlKey& key = form_control.key;
    const Deque<FormControlState>& queue = form_control.value;
    for (const FormControlState& form_control_state : queue) {
      state_vector.push_back(key.GetName());
      state_vector.push_back(key.GetType());
      form_control_state.SerializeTo(state_vector);
    }
  }
}

void SavedFormState::AppendControlState(const AtomicString& name,
                                        const AtomicString& type,
                                        const FormControlState& state) {
  ControlKey key(name.Impl(), type.Impl());
  ControlStateMap::iterator it = state_for_new_controls_.find(key);
  if (it != state_for_new_controls_.end()) {
    it->value.push_back(state);
  } else {
    Deque<FormControlState> state_list;
    state_list.push_back(state);
    state_for_new_controls_.Set(key, state_list);
  }
  control_state_count_++;
}

FormControlState SavedFormState::TakeControlState(const AtomicString& name,
                                                  const AtomicString& type) {
  if (state_for_new_controls_.empty())
    return FormControlState();
  ControlStateMap::iterator it =
      state_for_new_controls_.find(ControlKey(name.Impl(), type.Impl()));
  if (it == state_for_new_controls_.end())
    return FormControlState();
  DCHECK_GT(it->value.size(), 0u);
  FormControlState state = it->value.TakeFirst();
  control_state_count_--;
  if (it->value.empty())
    state_for_new_controls_.erase(it);
  return state;
}

Vector<String> SavedFormState::GetReferencedFilePaths() const {
  Vector<String> to_return;
  for (const auto& form_control : state_for_new_controls_) {
    const ControlKey& key = form_control.key;
    if (!Equal(key.GetType(), "file", 4))
      continue;
    const Deque<FormControlState>& queue = form_control.value;
    for (const FormControlState& form_control_state : queue) {
      to_return.AppendVector(
          HTMLInputElement::FilesFromFileInputFormControlState(
              form_control_state));
    }
  }
  return to_return;
}

// ----------------------------------------------------------------------------

class FormKeyGenerator final : public GarbageCollected<FormKeyGenerator> {
 public:
  FormKeyGenerator() = default;
  FormKeyGenerator(const FormKeyGenerator&) = delete;
  FormKeyGenerator& operator=(const FormKeyGenerator&) = delete;

  void Trace(Visitor* visitor) const { visitor->Trace(form_to_key_map_); }
  const AtomicString& FormKey(const ListedElement&);
  void WillDeleteForm(HTMLFormElement*);

 private:
  using FormToKeyMap = HeapHashMap<Member<HTMLFormElement>, AtomicString>;
  using FormSignatureToNextIndexMap = HashMap<String, unsigned>;
  FormToKeyMap form_to_key_map_;
  FormSignatureToNextIndexMap form_signature_to_next_index_map_;
};

static inline void RecordFormStructure(const HTMLFormElement& form,
                                       StringBuilder& builder) {
  // 2 is enough to distinguish forms in webkit.org/b/91209#c0
  const wtf_size_t kNamedControlsToBeRecorded = 2;
  const ListedElement::List& controls = form.ListedElements();
  builder.Append(" [");
  for (wtf_size_t i = 0, named_controls = 0;
       i < controls.size() && named_controls < kNamedControlsToBeRecorded;
       ++i) {
    ListedElement& control = *controls[i];
    if (!control.ClassSupportsStateRestore())
      continue;
    // The resultant string will be fragile if it contains a name of a
    // form-associated custom element. It's associated to the |form| only if its
    // custom element definition is available.  It's not associated if the
    // definition is unavailable though the element structure is identical.
    if (control.IsElementInternals())
      continue;
    if (!OwnerFormForState(control))
      continue;
    AtomicString name = control.GetName();
    if (name.empty())
      continue;
    named_controls++;
    builder.Append(name);
    builder.Append(' ');
  }
  builder.Append(']');
}

String FormSignature(const HTMLFormElement& form) {
  KURL action_url = form.GetURLAttributeAsKURL(html_names::kActionAttr);
  // Remove the query part because it might contain volatile parameters such
  // as a session key.
  if (!action_url.IsEmpty())
    action_url.SetQuery(String());

  StringBuilder builder;
  if (!action_url.IsEmpty())
    builder.Append(action_url.GetString());

  RecordFormStructure(form, builder);
  return builder.ToString();
}

const AtomicString& FormKeyGenerator::FormKey(const ListedElement& control) {
  HTMLFormElement* form = OwnerFormForState(control);
  if (!form) {
    DEFINE_STATIC_LOCAL(const AtomicString, form_key_for_no_owner,
                        ("No owner"));
    return form_key_for_no_owner;
  }
  FormToKeyMap::const_iterator it = form_to_key_map_.find(form);
  if (it != form_to_key_map_.end())
    return it->value;

  String signature = FormSignature(*form);
  DCHECK(!signature.IsNull());
  FormSignatureToNextIndexMap::AddResult result =
      form_signature_to_next_index_map_.insert(signature, 0);
  unsigned next_index = result.stored_value->value++;

  StringBuilder form_key_builder;
  form_key_builder.Append(signature);
  form_key_builder.Append(" #");
  form_key_builder.AppendNumber(next_index);
  FormToKeyMap::AddResult add_form_keyresult =
      form_to_key_map_.insert(form, form_key_builder.ToAtomicString());
  return add_form_keyresult.stored_value->value;
}

void FormKeyGenerator::WillDeleteForm(HTMLFormElement* form) {
  DCHECK(form);
  form_to_key_map_.erase(form);
}

// ----------------------------------------------------------------------------

DocumentState::DocumentState(Document& document) : document_(document) {}

void DocumentState::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(control_list_);
}

void DocumentState::InvalidateControlList() {
  if (is_control_list_dirty_)
    return;
  control_list_.resize(0);
  is_control_list_dirty_ = true;
}

const DocumentState::ControlList& DocumentState::GetControlList() {
  if (is_control_list_dirty_) {
    for (auto& element : Traversal<Element>::DescendantsOf(*document_)) {
      if (auto* control = ListedElement::From(element)) {
        if (control->ClassSupportsStateRestore())
          control_list_.push_back(control);
      }
    }
    is_control_list_dirty_ = false;
  }
  return control_list_;
}

static String FormStateSignature() {
  // In the legacy version of serialized state, the first item was a name
  // attribute value of a form control. The following string literal should
  // contain some characters which are rarely used for name attribute values.
  DEFINE_STATIC_LOCAL(String, signature,
                      ("\n\r?% Blink serialized form state version 10 \n\r=&"));
  return signature;
}

Vector<String> DocumentState::ToStateVector() {
  auto* key_generator = MakeGarbageCollected<FormKeyGenerator>();
  std::unique_ptr<SavedFormStateMap> state_map =
      base::WrapUnique(new SavedFormStateMap);
  for (auto& control : GetControlList()) {
    DCHECK(control->ToHTMLElement().isConnected());
    if (!control->ShouldSaveAndRestoreFormControlState())
      continue;
    SavedFormStateMap::AddResult result =
        state_map->insert(key_generator->FormKey(*control), nullptr);
    if (result.is_new_entry)
      result.stored_value->value = std::make_unique<SavedFormState>();
    result.stored_value->value->AppendControlState(
        control->GetName(), ControlType(*control),
        control->SaveFormControlState());
  }

  Vector<String> state_vector;
  state_vector.ReserveInitialCapacity(GetControlList().size() * 4);
  state_vector.push_back(FormStateSignature());
  for (const auto& saved_form_state : *state_map) {
    state_vector.push_back(saved_form_state.key);
    saved_form_state.value->SerializeTo(state_vector);
  }
  bool has_only_signature = state_vector.size() == 1;
  if (has_only_signature)
    state_vector.clear();
  return state_vector;
}

// ----------------------------------------------------------------------------

FormController::FormController(Document& document)
    : document_(document),
      document_state_(MakeGarbageCollected<DocumentState>(document)) {}

FormController::~FormController() = default;

void FormController::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(document_state_);
  visitor->Trace(form_key_generator_);
}

DocumentState* FormController::ControlStates() const {
  return document_state_.Get();
}

void FormController::SetStateForNewControls(
    const Vector<String>& state_vector) {
  ControlStatesFromStateVector(state_vector, saved_form_state_map_);
}

bool FormController::HasControlStates() const {
  return !saved_form_state_map_.empty();
}

FormControlState FormController::TakeStateForControl(
    const ListedElement& control) {
  if (saved_form_state_map_.empty())
    return FormControlState();
  if (!form_key_generator_)
    form_key_generator_ = MakeGarbageCollected<FormKeyGenerator>();
  SavedFormStateMap::iterator it =
      saved_form_state_map_.find(form_key_generator_->FormKey(control));
  if (it == saved_form_state_map_.end())
    return FormControlState();
  FormControlState state =
      it->value->TakeControlState(control.GetName(), ControlType(control));
  if (it->value->IsEmpty())
    saved_form_state_map_.erase(it);
  return state;
}

void FormController::ControlStatesFromStateVector(
    const Vector<String>& state_vector,
    SavedFormStateMap& map) {
  map.clear();

  wtf_size_t i = 0;
  if (state_vector.size() < 1 || state_vector[i++] != FormStateSignature())
    return;

  while (i + 1 < state_vector.size()) {
    AtomicString form_key = AtomicString(state_vector[i++]);
    std::unique_ptr<SavedFormState> state =
        SavedFormState::Deserialize(state_vector, i);
    if (!state) {
      i = 0;
      break;
    }
    map.insert(form_key, std::move(state));
  }
  if (i != state_vector.size())
    map.clear();
}

void FormController::WillDeleteForm(HTMLFormElement* form) {
  if (form_key_generator_)
    form_key_generator_->WillDeleteForm(form);
}

void FormController::RestoreControlStateFor(ListedElement& control) {
  if (!document_->HasFinishedParsing())
    return;
  if (OwnerFormForState(control))
    return;
  RestoreControlStateInternal(control);
}

void FormController::RestoreControlStateIn(HTMLFormElement& form) {
  if (!document_->HasFinishedParsing())
    return;
  EventQueueScope scope;
  const ListedElement::List& elements = form.ListedElements();
  for (const auto& control : elements) {
    if (!control->ClassSupportsStateRestore())
      continue;
    if (OwnerFormForState(*control) != &form)
      continue;
    RestoreControlStateInternal(*control);
  }
}

void FormController::RestoreControlStateInternal(ListedElement& control) {
  // We don't save state of a control with
  // ShouldSaveAndRestoreFormControlState() == false. But we need to skip
  // restoring process too because a control in another form might have the same
  // pair of name and type and saved its state.
  if (!control.ShouldSaveAndRestoreFormControlState())
    return;
  FormControlState state = TakeStateForControl(control);
  if (state.ValueSize() <= 0)
    return;
  HTMLElement& element = control.ToHTMLElement();
  if (element.IsDisabledFormControl() ||
      element.FastHasAttribute(html_names::kReadonlyAttr))
    return;
  // If a user already edited the control, we should not overwrite it.
  if (IsDirtyControl(control))
    return;
  // RestoreFormControlState might dispatch input/change events.
  control.RestoreFormControlState(state);
}

void FormController::RestoreControlStateOnUpgrade(ListedElement& control) {
  DCHECK(control.ClassSupportsStateRestore());
  if (!control.ShouldSaveAndRestoreFormControlState())
    return;
  FormControlState state = TakeStateForControl(control);
  if (state.ValueSize() > 0)
    control.RestoreFormControlState(state);
}

void FormController::ScheduleRestore() {
  document_->GetTaskRunner(TaskType::kInternalLoading)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(&FormController::RestoreAllControlsInDocumentOrder,
                        WrapPersistent(this)));
}

void FormController::RestoreImmediately() {
  if (did_restore_all_ || !HasControlStates())
    return;
  RestoreAllControlsInDocumentOrder();
}

void FormController::RestoreAllControlsInDocumentOrder() {
  if (!document_->IsActive() || did_restore_all_)
    return;
  HeapHashSet<Member<HTMLFormElement>> finished_forms;
  EventQueueScope scope;
  for (auto& control : document_state_->GetControlList()) {
    auto* owner = OwnerFormForState(*control);
    if (!owner)
      RestoreControlStateFor(*control);
    else if (finished_forms.insert(owner).is_new_entry)
      RestoreControlStateIn(*owner);
  }
  did_restore_all_ = true;
}

Vector<String> FormController::GetReferencedFilePaths(
    const Vector<String>& state_vector) {
  Vector<String> to_return;
  SavedFormStateMap map;
  ControlStatesFromStateVector(state_vector, map);
  for (const auto& saved_form_state : map)
    to_return.AppendVector(saved_form_state.value->GetReferencedFilePaths());
  return to_return;
}

void FormController::InvalidateStatefulFormControlList() {
  document_state_->InvalidateControlList();
}

}  // namespace blink
