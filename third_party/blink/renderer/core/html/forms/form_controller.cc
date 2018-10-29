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

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/html/forms/file_chooser.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element_with_state.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using namespace HTMLNames;

static inline HTMLFormElement* OwnerFormForState(
    const HTMLFormControlElementWithState& control) {
  // Assume controls with form attribute have no owners because we restore
  // state during parsing and form owners of such controls might be
  // indeterminate.
  return control.FastHasAttribute(formAttr) ? nullptr : control.Form();
}

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
  state.values_.ReserveCapacity(value_size);
  for (unsigned i = 0; i < value_size; ++i)
    state.Append(state_vector[index++]);
  return state;
}

// ----------------------------------------------------------------------------

class FormElementKey {
 public:
  FormElementKey(StringImpl* = nullptr, StringImpl* = nullptr);
  ~FormElementKey();
  FormElementKey(const FormElementKey&);
  FormElementKey& operator=(const FormElementKey&);

  StringImpl* GetName() const { return name_; }
  StringImpl* GetType() const { return type_; }

  // Hash table deleted values, which are only constructed and never copied or
  // destroyed.
  FormElementKey(WTF::HashTableDeletedValueType)
      : name_(HashTableDeletedValue()) {}
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

FormElementKey::FormElementKey(StringImpl* name, StringImpl* type)
    : name_(name), type_(type) {
  Ref();
}

FormElementKey::~FormElementKey() {
  Deref();
}

FormElementKey::FormElementKey(const FormElementKey& other)
    : name_(other.GetName()), type_(other.GetType()) {
  Ref();
}

FormElementKey& FormElementKey::operator=(const FormElementKey& other) {
  other.Ref();
  Deref();
  name_ = other.GetName();
  type_ = other.GetType();
  return *this;
}

void FormElementKey::Ref() const {
  if (GetName())
    GetName()->AddRef();
  if (GetType())
    GetType()->AddRef();
}

void FormElementKey::Deref() const {
  if (GetName())
    GetName()->Release();
  if (GetType())
    GetType()->Release();
}

inline bool operator==(const FormElementKey& a, const FormElementKey& b) {
  return a.GetName() == b.GetName() && a.GetType() == b.GetType();
}

struct FormElementKeyHash {
  static unsigned GetHash(const FormElementKey&);
  static bool Equal(const FormElementKey& a, const FormElementKey& b) {
    return a == b;
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

unsigned FormElementKeyHash::GetHash(const FormElementKey& key) {
  return StringHasher::HashMemory<sizeof(FormElementKey)>(&key);
}

struct FormElementKeyHashTraits : WTF::GenericHashTraits<FormElementKey> {
  static void ConstructDeletedValue(FormElementKey& slot, bool) {
    new (NotNull, &slot) FormElementKey(WTF::kHashTableDeletedValue);
  }
  static bool IsDeletedValue(const FormElementKey& value) {
    return value.IsHashTableDeletedValue();
  }
};

// ----------------------------------------------------------------------------

class SavedFormState {
  USING_FAST_MALLOC(SavedFormState);

 public:
  static std::unique_ptr<SavedFormState> Create();
  static std::unique_ptr<SavedFormState> Deserialize(const Vector<String>&,
                                                     wtf_size_t& index);
  void SerializeTo(Vector<String>&) const;
  bool IsEmpty() const { return state_for_new_form_elements_.IsEmpty(); }
  void AppendControlState(const AtomicString& name,
                          const AtomicString& type,
                          const FormControlState&);
  FormControlState TakeControlState(const AtomicString& name,
                                    const AtomicString& type);

  Vector<String> GetReferencedFilePaths() const;

 private:
  SavedFormState() : control_state_count_(0) {}

  using FormElementStateMap = HashMap<FormElementKey,
                                      Deque<FormControlState>,
                                      FormElementKeyHash,
                                      FormElementKeyHashTraits>;
  FormElementStateMap state_for_new_form_elements_;
  wtf_size_t control_state_count_;

  DISALLOW_COPY_AND_ASSIGN(SavedFormState);
};

std::unique_ptr<SavedFormState> SavedFormState::Create() {
  return base::WrapUnique(new SavedFormState);
}

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
    if (type.IsEmpty() ||
        type.Find(IsNotFormControlTypeCharacter) != kNotFound ||
        state.IsFailure())
      return nullptr;
    saved_form_state->AppendControlState(AtomicString(name), AtomicString(type),
                                         state);
  }
  return saved_form_state;
}

void SavedFormState::SerializeTo(Vector<String>& state_vector) const {
  state_vector.push_back(String::Number(control_state_count_));
  for (const auto& form_control : state_for_new_form_elements_) {
    const FormElementKey& key = form_control.key;
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
  FormElementKey key(name.Impl(), type.Impl());
  FormElementStateMap::iterator it = state_for_new_form_elements_.find(key);
  if (it != state_for_new_form_elements_.end()) {
    it->value.push_back(state);
  } else {
    Deque<FormControlState> state_list;
    state_list.push_back(state);
    state_for_new_form_elements_.Set(key, state_list);
  }
  control_state_count_++;
}

FormControlState SavedFormState::TakeControlState(const AtomicString& name,
                                                  const AtomicString& type) {
  if (state_for_new_form_elements_.IsEmpty())
    return FormControlState();
  FormElementStateMap::iterator it = state_for_new_form_elements_.find(
      FormElementKey(name.Impl(), type.Impl()));
  if (it == state_for_new_form_elements_.end())
    return FormControlState();
  DCHECK_GT(it->value.size(), 0u);
  FormControlState state = it->value.TakeFirst();
  control_state_count_--;
  if (!it->value.size())
    state_for_new_form_elements_.erase(it);
  return state;
}

Vector<String> SavedFormState::GetReferencedFilePaths() const {
  Vector<String> to_return;
  for (const auto& form_control : state_for_new_form_elements_) {
    const FormElementKey& key = form_control.key;
    if (!Equal(key.GetType(), "file", 4))
      continue;
    const Deque<FormControlState>& queue = form_control.value;
    for (const FormControlState& form_control_state : queue) {
      const FileChooserFileInfoList& selected_files =
          HTMLInputElement::FilesFromFileInputFormControlState(
              form_control_state);
      for (const auto& file : selected_files) {
        to_return.push_back(
            FilePathToString(file->get_native_file()->file_path));
      }
    }
  }
  return to_return;
}

// ----------------------------------------------------------------------------

class FormKeyGenerator final
    : public GarbageCollectedFinalized<FormKeyGenerator> {

 public:
  static FormKeyGenerator* Create() { return new FormKeyGenerator; }
  void Trace(blink::Visitor* visitor) { visitor->Trace(form_to_key_map_); }
  const AtomicString& FormKey(const HTMLFormControlElementWithState&);
  void WillDeleteForm(HTMLFormElement*);

 private:
  FormKeyGenerator() = default;

  using FormToKeyMap = HeapHashMap<Member<HTMLFormElement>, AtomicString>;
  using FormSignatureToNextIndexMap = HashMap<String, unsigned>;
  FormToKeyMap form_to_key_map_;
  FormSignatureToNextIndexMap form_signature_to_next_index_map_;

  DISALLOW_COPY_AND_ASSIGN(FormKeyGenerator);
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
    if (!controls[i]->IsFormControlElementWithState())
      continue;
    HTMLFormControlElementWithState* control =
        ToHTMLFormControlElementWithState(controls[i]);
    if (!OwnerFormForState(*control))
      continue;
    AtomicString name = control->GetName();
    if (name.IsEmpty())
      continue;
    named_controls++;
    builder.Append(name);
    builder.Append(' ');
  }
  builder.Append(']');
}

static inline String FormSignature(const HTMLFormElement& form) {
  KURL action_url = form.GetURLAttribute(actionAttr);
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

const AtomicString& FormKeyGenerator::FormKey(
    const HTMLFormControlElementWithState& control) {
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

DocumentState* DocumentState::Create() {
  return new DocumentState;
}

void DocumentState::Trace(blink::Visitor* visitor) {
  visitor->Trace(form_controls_);
}

void DocumentState::AddControl(HTMLFormControlElementWithState* control) {
  DCHECK(!control->Next() && !control->Prev());
  form_controls_.Append(control);
}

void DocumentState::RemoveControl(HTMLFormControlElementWithState* control) {
  form_controls_.Remove(control);
  control->SetPrev(nullptr);
  control->SetNext(nullptr);
}

static String FormStateSignature() {
  // In the legacy version of serialized state, the first item was a name
  // attribute value of a form control. The following string literal should
  // contain some characters which are rarely used for name attribute values.
  DEFINE_STATIC_LOCAL(String, signature,
                      ("\n\r?% Blink serialized form state version 9 \n\r=&"));
  return signature;
}

Vector<String> DocumentState::ToStateVector() {
  FormKeyGenerator* key_generator = FormKeyGenerator::Create();
  std::unique_ptr<SavedFormStateMap> state_map =
      base::WrapUnique(new SavedFormStateMap);
  for (HTMLFormControlElementWithState* control = form_controls_.Head();
       control; control = control->Next()) {
    DCHECK(control->isConnected());
    if (!control->ShouldSaveAndRestoreFormControlState())
      continue;
    SavedFormStateMap::AddResult result =
        state_map->insert(key_generator->FormKey(*control), nullptr);
    if (result.is_new_entry)
      result.stored_value->value = SavedFormState::Create();
    result.stored_value->value->AppendControlState(
        control->GetName(), control->type(), control->SaveFormControlState());
  }

  Vector<String> state_vector;
  state_vector.ReserveInitialCapacity(form_controls_.size() * 4);
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

FormController::FormController() : document_state_(DocumentState::Create()) {}

FormController::~FormController() = default;

void FormController::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_state_);
  visitor->Trace(form_key_generator_);
}

DocumentState* FormController::FormElementsState() const {
  return document_state_.Get();
}

void FormController::SetStateForNewFormElements(
    const Vector<String>& state_vector) {
  FormStatesFromStateVector(state_vector, saved_form_state_map_);
}

bool FormController::HasFormStates() const {
  return !saved_form_state_map_.IsEmpty();
}

FormControlState FormController::TakeStateForFormElement(
    const HTMLFormControlElementWithState& control) {
  if (saved_form_state_map_.IsEmpty())
    return FormControlState();
  if (!form_key_generator_)
    form_key_generator_ = FormKeyGenerator::Create();
  SavedFormStateMap::iterator it =
      saved_form_state_map_.find(form_key_generator_->FormKey(control));
  if (it == saved_form_state_map_.end())
    return FormControlState();
  FormControlState state =
      it->value->TakeControlState(control.GetName(), control.type());
  if (it->value->IsEmpty())
    saved_form_state_map_.erase(it);
  return state;
}

void FormController::FormStatesFromStateVector(
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

void FormController::RestoreControlStateFor(
    HTMLFormControlElementWithState& control) {
  // We don't save state of a control with
  // shouldSaveAndRestoreFormControlState() == false. But we need to skip
  // restoring process too because a control in another form might have the same
  // pair of name and type and saved its state.
  if (!control.ShouldSaveAndRestoreFormControlState())
    return;
  if (OwnerFormForState(control))
    return;
  FormControlState state = TakeStateForFormElement(control);
  if (state.ValueSize() > 0)
    control.RestoreFormControlState(state);
}

void FormController::RestoreControlStateIn(HTMLFormElement& form) {
  EventQueueScope scope;
  const ListedElement::List& elements = form.ListedElements();
  for (const auto& element : elements) {
    if (!element->IsFormControlElementWithState())
      continue;
    HTMLFormControlElementWithState* control =
        ToHTMLFormControlElementWithState(element);
    if (!control->ShouldSaveAndRestoreFormControlState())
      continue;
    if (OwnerFormForState(*control) != &form)
      continue;
    FormControlState state = TakeStateForFormElement(*control);
    if (state.ValueSize() > 0) {
      // restoreFormControlState might dispatch input/change events.
      control->RestoreFormControlState(state);
    }
  }
}

Vector<String> FormController::GetReferencedFilePaths(
    const Vector<String>& state_vector) {
  Vector<String> to_return;
  SavedFormStateMap map;
  FormStatesFromStateVector(state_vector, map);
  for (const auto& saved_form_state : map)
    to_return.AppendVector(saved_form_state.value->GetReferencedFilePaths());
  return to_return;
}

void FormController::RegisterStatefulFormControl(
    HTMLFormControlElementWithState& control) {
  document_state_->AddControl(&control);
}

void FormController::UnregisterStatefulFormControl(
    HTMLFormControlElementWithState& control) {
  document_state_->RemoveControl(&control);
}

}  // namespace blink
