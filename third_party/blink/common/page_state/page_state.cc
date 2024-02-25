// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page_state/page_state.h"

#include <stddef.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace blink {
namespace {

base::FilePath ToFilePath(const std::optional<std::u16string>& s) {
  return s ? base::FilePath::FromUTF16Unsafe(*s) : base::FilePath();
}

void ToFilePathVector(const std::vector<std::optional<std::u16string>>& input,
                      std::vector<base::FilePath>* output) {
  output->clear();
  output->reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i)
    output->emplace_back(ToFilePath(input[i]));
}

PageState ToPageState(const ExplodedPageState& state) {
  std::string encoded_data;
  EncodePageState(state, &encoded_data);
  return PageState::CreateFromEncodedData(encoded_data);
}

void RecursivelyRemovePasswordData(ExplodedFrameState* state) {
  if (state->http_body.contains_passwords)
    state->http_body = ExplodedHttpBody();
}

void RecursivelyRemoveScrollOffset(ExplodedFrameState* state) {
  state->scroll_offset = gfx::Point();
  state->visual_viewport_scroll_offset = gfx::PointF();
}

void RecursivelyRemoveReferrer(ExplodedFrameState* state) {
  state->referrer.reset();
  state->referrer_policy = network::mojom::ReferrerPolicy::kDefault;
  for (std::vector<ExplodedFrameState>::iterator it = state->children.begin();
       it != state->children.end(); ++it) {
    RecursivelyRemoveReferrer(&*it);
  }
}

}  // namespace

// static
PageState PageState::CreateFromEncodedData(const std::string& data) {
  return PageState(data);
}

// static
PageState PageState::CreateFromURL(const GURL& url) {
  ExplodedPageState state;

  state.top.url_string = base::UTF8ToUTF16(url.possibly_invalid_spec());

  return ToPageState(state);
}

// static
PageState PageState::CreateForTesting(
    const GURL& url,
    bool body_contains_password_data,
    const char* optional_body_data,
    const base::FilePath* optional_body_file_path) {
  ExplodedPageState state;

  state.top.url_string = base::UTF8ToUTF16(url.possibly_invalid_spec());

  if (optional_body_data || optional_body_file_path) {
    if (optional_body_data) {
      std::string body_data(optional_body_data);
      state.top.http_body.request_body = new network::ResourceRequestBody();
      state.top.http_body.request_body->AppendBytes(body_data.data(),
                                                    body_data.size());
    }
    if (optional_body_file_path) {
      state.top.http_body.request_body = new network::ResourceRequestBody();
      state.top.http_body.request_body->AppendFileRange(
          *optional_body_file_path, 0, std::numeric_limits<uint64_t>::max(),
          base::Time());
      state.referenced_files.emplace_back(
          optional_body_file_path->AsUTF16Unsafe());
    }
    state.top.http_body.contains_passwords = body_contains_password_data;
  }

  return ToPageState(state);
}

// static
PageState PageState::CreateForTestingWithSequenceNumbers(
    const GURL& url,
    int64_t item_sequence_number,
    int64_t document_sequence_number) {
  ExplodedPageState page_state;
  page_state.top.url_string = base::UTF8ToUTF16(url.spec());
  page_state.top.item_sequence_number = item_sequence_number;
  page_state.top.document_sequence_number = document_sequence_number;

  std::string encoded_page_state;
  EncodePageState(page_state, &encoded_page_state);
  return CreateFromEncodedData(encoded_page_state);
}

PageState::PageState() {}

bool PageState::IsValid() const {
  return !data_.empty();
}

bool PageState::Equals(const PageState& other) const {
  return data_ == other.data_;
}

const std::string& PageState::ToEncodedData() const {
  return data_;
}

std::vector<base::FilePath> PageState::GetReferencedFiles() const {
  std::vector<base::FilePath> results;

  ExplodedPageState state;
  if (DecodePageState(data_, &state))
    ToFilePathVector(state.referenced_files, &results);

  return results;
}

PageState PageState::RemovePasswordData() const {
  ExplodedPageState state;
  if (!DecodePageState(data_, &state))
    return PageState();  // Oops!

  RecursivelyRemovePasswordData(&state.top);

  return ToPageState(state);
}

PageState PageState::RemoveScrollOffset() const {
  ExplodedPageState state;
  if (!DecodePageState(data_, &state))
    return PageState();  // Oops!

  RecursivelyRemoveScrollOffset(&state.top);

  return ToPageState(state);
}

PageState PageState::RemoveReferrer() const {
  if (data_.empty())
    return *this;

  ExplodedPageState state;
  if (!DecodePageState(data_, &state))
    return PageState();  // Oops!

  RecursivelyRemoveReferrer(&state.top);

  return ToPageState(state);
}

PageState::PageState(const std::string& data) : data_(data) {
  // TODO(darin): Enable this DCHECK once tests have been fixed up to not pass
  // bogus encoded data to CreateFromEncodedData.
  // DCHECK(IsValid());
}

void PageState::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("data", data_);
}

}  // namespace blink
