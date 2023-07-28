/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/form_data.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_file_usvstring.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/line_ending.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class FormDataIterationSource final
    : public PairSyncIterable<FormData>::IterationSource {
 public:
  FormDataIterationSource(FormData* form_data)
      : form_data_(form_data), current_(0) {}

  bool FetchNextItem(ScriptState* script_state,
                     String& name,
                     V8FormDataEntryValue*& value,
                     ExceptionState& exception_state) override {
    if (current_ >= form_data_->size())
      return false;

    const FormData::Entry& entry = *form_data_->Entries()[current_++];
    name = entry.name();
    if (entry.IsString()) {
      value = MakeGarbageCollected<V8FormDataEntryValue>(entry.Value());
    } else {
      DCHECK(entry.isFile());
      value = MakeGarbageCollected<V8FormDataEntryValue>(entry.GetFile());
    }
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(form_data_);
    PairSyncIterable<FormData>::IterationSource::Trace(visitor);
  }

 private:
  const Member<FormData> form_data_;
  wtf_size_t current_;
};

}  // namespace

FormData::FormData(const WTF::TextEncoding& encoding) : encoding_(encoding) {}

FormData::FormData(const FormData& form_data)
    : encoding_(form_data.encoding_),
      entries_(form_data.entries_),
      contains_password_data_(form_data.contains_password_data_) {}

FormData::FormData() : encoding_(UTF8Encoding()) {}

FormData* FormData::Create(HTMLFormElement* form,
                           ExceptionState& exception_state) {
  return FormData::Create(form, nullptr, exception_state);
}

// https://xhr.spec.whatwg.org/#dom-formdata
FormData* FormData::Create(HTMLFormElement* form,
                           HTMLElement* submitter,
                           ExceptionState& exception_state) {
  if (!form) {
    return MakeGarbageCollected<FormData>();
  }
  // 1. If form is given, then:
  HTMLFormControlElement* control = nullptr;
  // 1.1. If submitter is non-null, then:
  if (submitter) {
    // 1.1.1. If submitter is not a submit button, then throw a TypeError.
    control = DynamicTo<HTMLFormControlElement>(submitter);
    if (!control || !control->CanBeSuccessfulSubmitButton()) {
      exception_state.ThrowTypeError(
          "The specified element is not a submit button.");
      return nullptr;
    }
    // 1.1.2. If submitter's form owner is not this form element, then throw a
    // "NotFoundError" DOMException.
    if (control->formOwner() != form) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotFoundError,
          "The specified element is not owned by this form element.");
      return nullptr;
    }
  }
  // 1.2. Let list be the result of constructing the entry list for form and
  // submitter.
  FormData* form_data = form->ConstructEntryList(control, UTF8Encoding());
  // 1.3. If list is null, then throw an "InvalidStateError" DOMException.
  if (!form_data) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The form is constructing entry list.");
    return nullptr;
  }
  // 1.4. Set this’s entry list to list.
  // Return a shallow copy of |form_data| because |form_data| is visible in
  // "formdata" event, and the specification says it should be different from
  // the FormData object to be returned.
  return MakeGarbageCollected<FormData>(*form_data);
}

void FormData::Trace(Visitor* visitor) const {
  visitor->Trace(entries_);
  ScriptWrappable::Trace(visitor);
}

void FormData::append(const String& name, const String& value) {
  entries_.push_back(MakeGarbageCollected<Entry>(name, value));
}

void FormData::append(ScriptState* script_state,
                      const String& name,
                      Blob* blob,
                      const String& filename) {
  if (!blob) {
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kFormDataAppendNull);
  }
  append(name, blob, filename);
}

void FormData::deleteEntry(const String& name) {
  wtf_size_t i = 0;
  while (i < entries_.size()) {
    if (entries_[i]->name() == name) {
      entries_.EraseAt(i);
    } else {
      ++i;
    }
  }
}

V8FormDataEntryValue* FormData::get(const String& name) {
  for (const auto& entry : Entries()) {
    if (entry->name() == name) {
      if (entry->IsString()) {
        return MakeGarbageCollected<V8FormDataEntryValue>(entry->Value());
      } else {
        DCHECK(entry->isFile());
        return MakeGarbageCollected<V8FormDataEntryValue>(entry->GetFile());
      }
    }
  }
  return nullptr;
}

HeapVector<Member<V8FormDataEntryValue>> FormData::getAll(const String& name) {
  HeapVector<Member<V8FormDataEntryValue>> results;

  for (const auto& entry : Entries()) {
    if (entry->name() != name)
      continue;
    V8FormDataEntryValue* value;
    if (entry->IsString()) {
      value = MakeGarbageCollected<V8FormDataEntryValue>(entry->Value());
    } else {
      DCHECK(entry->isFile());
      value = MakeGarbageCollected<V8FormDataEntryValue>(entry->GetFile());
    }
    results.push_back(value);
  }
  return results;
}

bool FormData::has(const String& name) {
  for (const auto& entry : Entries()) {
    if (entry->name() == name)
      return true;
  }
  return false;
}

void FormData::set(const String& name, const String& value) {
  SetEntry(MakeGarbageCollected<Entry>(name, value));
}

void FormData::set(const String& name, Blob* blob, const String& filename) {
  SetEntry(MakeGarbageCollected<Entry>(name, blob, filename));
}

void FormData::SetEntry(const Entry* entry) {
  DCHECK(entry);
  bool found = false;
  wtf_size_t i = 0;
  while (i < entries_.size()) {
    if (entries_[i]->name() != entry->name()) {
      ++i;
    } else if (found) {
      entries_.EraseAt(i);
    } else {
      found = true;
      entries_[i] = entry;
      ++i;
    }
  }
  if (!found)
    entries_.push_back(entry);
}

void FormData::append(const String& name, Blob* blob, const String& filename) {
  entries_.push_back(MakeGarbageCollected<Entry>(name, blob, filename));
}

void FormData::AppendFromElement(const String& name, int value) {
  append(ReplaceUnmatchedSurrogates(name), String::Number(value));
}

void FormData::AppendFromElement(const String& name, File* file) {
  entries_.push_back(MakeGarbageCollected<Entry>(
      ReplaceUnmatchedSurrogates(name), file, String()));
}

void FormData::AppendFromElement(const String& name, const String& value) {
  entries_.push_back(MakeGarbageCollected<Entry>(
      ReplaceUnmatchedSurrogates(name), ReplaceUnmatchedSurrogates(value)));
}

std::string FormData::Encode(const String& string) const {
  return encoding_.Encode(string, WTF::kEntitiesForUnencodables);
}

scoped_refptr<EncodedFormData> FormData::EncodeFormData(
    EncodedFormData::EncodingType encoding_type) {
  scoped_refptr<EncodedFormData> form_data = EncodedFormData::Create();
  Vector<char> encoded_data;
  for (const auto& entry : Entries()) {
    FormDataEncoder::AddKeyValuePairAsFormData(
        encoded_data, Encode(entry->name()),
        entry->isFile()
            ? Encode(ReplaceUnmatchedSurrogates(entry->GetFile()->name()))
            : Encode(entry->Value()),
        encoding_type);
  }
  form_data->AppendData(encoded_data.data(), encoded_data.size());
  return form_data;
}

scoped_refptr<EncodedFormData> FormData::EncodeMultiPartFormData() {
  scoped_refptr<EncodedFormData> form_data = EncodedFormData::Create();
  form_data->SetBoundary(FormDataEncoder::GenerateUniqueBoundaryString());
  Vector<char> encoded_data;
  for (const auto& entry : Entries()) {
    Vector<char> header;
    FormDataEncoder::BeginMultiPartHeader(header, form_data->Boundary().data(),
                                          Encode(entry->name()));

    // If the current type is blob, then we also need to include the
    // filename.
    if (entry->GetBlob()) {
      String name;
      if (auto* file = DynamicTo<File>(entry->GetBlob())) {
        // For file blob, use the filename (or relative path if it is
        // present) as the name.
        name = file->webkitRelativePath().empty() ? file->name()
                                                  : file->webkitRelativePath();

        // If a filename is passed in FormData.append(), use it instead
        // of the file blob's name.
        if (!entry->Filename().IsNull())
          name = entry->Filename();
      } else {
        // For non-file blob, use the filename if it is passed in
        // FormData.append().
        if (!entry->Filename().IsNull())
          name = entry->Filename();
        else
          name = "blob";
      }

      // We have to include the filename=".." part in the header, even if
      // the filename is empty.
      FormDataEncoder::AddFilenameToMultiPartHeader(header, Encoding(), name);

      // Add the content type if available, or "application/octet-stream"
      // otherwise (RFC 1867).
      String content_type;
      if (entry->GetBlob()->type().empty())
        content_type = "application/octet-stream";
      else
        content_type = entry->GetBlob()->type();
      FormDataEncoder::AddContentTypeToMultiPartHeader(header, content_type);
    }

    FormDataEncoder::FinishMultiPartHeader(header);

    // Append body
    form_data->AppendData(header.data(), header.size());
    if (entry->GetBlob()) {
      if (entry->GetBlob()->HasBackingFile()) {
        auto* file = To<File>(entry->GetBlob());
        // Do not add the file if the path is empty.
        if (!file->GetPath().empty())
          form_data->AppendFile(file->GetPath(), file->LastModifiedTime());
      } else {
        form_data->AppendBlob(entry->GetBlob()->Uuid(),
                              entry->GetBlob()->GetBlobDataHandle());
      }
    } else {
      std::string encoded_value =
          Encode(NormalizeLineEndingsToCRLF(entry->Value()));
      form_data->AppendData(
          encoded_value.c_str(),
          base::checked_cast<wtf_size_t>(encoded_value.length()));
    }
    form_data->AppendData("\r\n", 2);
  }
  FormDataEncoder::AddBoundaryToMultiPartHeader(
      encoded_data, form_data->Boundary().data(), true);
  form_data->AppendData(encoded_data.data(), encoded_data.size());
  return form_data;
}

PairSyncIterable<FormData>::IterationSource* FormData::CreateIterationSource(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<FormDataIterationSource>(this);
}

// ----------------------------------------------------------------

FormData::Entry::Entry(const String& name, const String& value)
    : name_(name), value_(value) {
  DCHECK_EQ(name, ReplaceUnmatchedSurrogates(name))
      << "'name' should be a USVString.";
  DCHECK_EQ(value, ReplaceUnmatchedSurrogates(value))
      << "'value' should be a USVString.";
}

FormData::Entry::Entry(const String& name, Blob* blob, const String& filename)
    : name_(name), blob_(blob), filename_(filename) {
  DCHECK_EQ(name, ReplaceUnmatchedSurrogates(name))
      << "'name' should be a USVString.";
}

void FormData::Entry::Trace(Visitor* visitor) const {
  visitor->Trace(blob_);
}

File* FormData::Entry::GetFile() const {
  DCHECK(GetBlob());
  // The spec uses the passed filename when inserting entries into the list.
  // Here, we apply the filename (if present) as an override when extracting
  // entries.
  // FIXME: Consider applying the name during insertion.

  if (auto* file = DynamicTo<File>(GetBlob())) {
    if (Filename().IsNull())
      return file;
    return file->Clone(Filename());
  }

  String filename = filename_;
  if (filename.IsNull())
    filename = "blob";
  return MakeGarbageCollected<File>(filename, base::Time::Now(),
                                    GetBlob()->GetBlobDataHandle());
}

void FormData::AppendToControlState(FormControlState& state) const {
  state.Append(String::Number(size()));
  for (const auto& entry : Entries()) {
    state.Append(entry->name());
    if (entry->isFile()) {
      state.Append("File");
      entry->GetFile()->AppendToControlState(state);
    } else {
      state.Append("USVString");
      state.Append(entry->Value());
    }
  }
}

FormData* FormData::CreateFromControlState(ExecutionContext& execution_context,
                                           const FormControlState& state,
                                           wtf_size_t& index) {
  bool ok = false;
  uint64_t length = state[index].ToUInt64Strict(&ok);
  if (!ok)
    return nullptr;
  auto* form_data = MakeGarbageCollected<FormData>();
  ++index;
  for (uint64_t j = 0; j < length; ++j) {
    // Need at least three items.
    if (index + 2 >= state.ValueSize())
      return nullptr;
    const String& name = state[index++];
    const String& entry_type = state[index++];
    if (entry_type == "File") {
      if (auto* file =
              File::CreateFromControlState(&execution_context, state, index)) {
        form_data->append(name, file);
      } else {
        return nullptr;
      }
    } else if (entry_type == "USVString") {
      form_data->append(name, state[index++]);
    } else {
      return nullptr;
    }
  }
  return form_data;
}

}  // namespace blink
