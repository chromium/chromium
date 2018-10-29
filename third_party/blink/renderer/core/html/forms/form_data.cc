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

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/text/line_ending.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class FormDataIterationSource final
    : public PairIterable<String, FormDataEntryValue>::IterationSource {
 public:
  FormDataIterationSource(FormData* form_data)
      : form_data_(form_data), current_(0) {}

  bool Next(ScriptState* script_state,
            String& name,
            FormDataEntryValue& value,
            ExceptionState& exception_state) override {
    if (current_ >= form_data_->size())
      return false;

    const FormData::Entry& entry = *form_data_->Entries()[current_++];
    name = entry.name();
    if (entry.IsString()) {
      value.SetUSVString(entry.Value());
    } else {
      DCHECK(entry.isFile());
      value.SetFile(entry.GetFile());
    }
    return true;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(form_data_);
    PairIterable<String, FormDataEntryValue>::IterationSource::Trace(visitor);
  }

 private:
  const Member<FormData> form_data_;
  wtf_size_t current_;
};

String Normalize(const String& input) {
  // https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#append-an-entry
  return ReplaceUnmatchedSurrogates(NormalizeLineEndingsToCRLF(input));
}

}  // namespace

FormData::FormData(const WTF::TextEncoding& encoding) : encoding_(encoding) {}

FormData::FormData(HTMLFormElement* form) : encoding_(UTF8Encoding()) {
  if (form)
    form->ConstructFormDataSet(nullptr, *this);
}

void FormData::Trace(blink::Visitor* visitor) {
  visitor->Trace(entries_);
  ScriptWrappable::Trace(visitor);
}

void FormData::append(const String& name, const String& value) {
  entries_.push_back(new Entry(name, value));
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

void FormData::get(const String& name, FormDataEntryValue& result) {
  for (const auto& entry : Entries()) {
    if (entry->name() == name) {
      if (entry->IsString()) {
        result.SetUSVString(entry->Value());
      } else {
        DCHECK(entry->isFile());
        result.SetFile(entry->GetFile());
      }
      return;
    }
  }
}

HeapVector<FormDataEntryValue> FormData::getAll(const String& name) {
  HeapVector<FormDataEntryValue> results;

  for (const auto& entry : Entries()) {
    if (entry->name() != name)
      continue;
    FormDataEntryValue value;
    if (entry->IsString()) {
      value.SetUSVString(entry->Value());
    } else {
      DCHECK(entry->isFile());
      value.SetFile(entry->GetFile());
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
  SetEntry(new Entry(name, value));
}

void FormData::set(const String& name, Blob* blob, const String& filename) {
  SetEntry(new Entry(name, blob, filename));
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
  entries_.push_back(new Entry(name, blob, filename));
}

void FormData::AppendFromElement(const String& name, int value) {
  append(Normalize(name), String::Number(value));
}

void FormData::AppendFromElement(const String& name, File* file) {
  entries_.push_back(new Entry(Normalize(name), file, String()));
}

void FormData::AppendFromElement(const String& name, const String& value) {
  entries_.push_back(new Entry(Normalize(name), Normalize(value)));
}

CString FormData::Encode(const String& string) const {
  return encoding_.Encode(string, WTF::kEntitiesForUnencodables);
}

scoped_refptr<EncodedFormData> FormData::EncodeFormData(
    EncodedFormData::EncodingType encoding_type) {
  scoped_refptr<EncodedFormData> form_data = EncodedFormData::Create();
  Vector<char> encoded_data;
  for (const auto& entry : Entries()) {
    FormDataEncoder::AddKeyValuePairAsFormData(
        encoded_data, Encode(entry->name()),
        entry->isFile() ? Encode(Normalize(entry->GetFile()->name()))
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
      if (entry->GetBlob()->IsFile()) {
        File* file = ToFile(entry->GetBlob());
        // For file blob, use the filename (or relative path if it is
        // present) as the name.
        name = file->webkitRelativePath().IsEmpty()
                   ? file->name()
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
      if (entry->GetBlob()->type().IsEmpty())
        content_type = "application/octet-stream";
      else
        content_type = entry->GetBlob()->type();
      FormDataEncoder::AddContentTypeToMultiPartHeader(header,
                                                       content_type.Latin1());
    }

    FormDataEncoder::FinishMultiPartHeader(header);

    // Append body
    form_data->AppendData(header.data(), header.size());
    if (entry->GetBlob()) {
      if (entry->GetBlob()->HasBackingFile()) {
        File* file = ToFile(entry->GetBlob());
        // Do not add the file if the path is empty.
        if (!file->GetPath().IsEmpty())
          form_data->AppendFile(file->GetPath());
      } else {
        form_data->AppendBlob(entry->GetBlob()->Uuid(),
                              entry->GetBlob()->GetBlobDataHandle());
      }
    } else {
      CString encoded_value = Encode(entry->Value());
      form_data->AppendData(encoded_value.data(), encoded_value.length());
    }
    form_data->AppendData("\r\n", 2);
  }
  FormDataEncoder::AddBoundaryToMultiPartHeader(
      encoded_data, form_data->Boundary().data(), true);
  form_data->AppendData(encoded_data.data(), encoded_data.size());
  return form_data;
}

PairIterable<String, FormDataEntryValue>::IterationSource*
FormData::StartIteration(ScriptState*, ExceptionState&) {
  return new FormDataIterationSource(this);
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

void FormData::Entry::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_);
}

File* FormData::Entry::GetFile() const {
  DCHECK(GetBlob());
  // The spec uses the passed filename when inserting entries into the list.
  // Here, we apply the filename (if present) as an override when extracting
  // entries.
  // FIXME: Consider applying the name during insertion.

  if (GetBlob()->IsFile()) {
    File* file = ToFile(GetBlob());
    if (Filename().IsNull())
      return file;
    return file->Clone(Filename());
  }

  String filename = filename_;
  if (filename.IsNull())
    filename = "blob";
  return File::Create(filename, CurrentTimeMS(),
                      GetBlob()->GetBlobDataHandle());
}

}  // namespace blink
