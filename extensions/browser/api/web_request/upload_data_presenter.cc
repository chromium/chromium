// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/upload_data_presenter.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/browser/api/web_request/form_data_parser.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "net/base/upload_file_element_reader.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace keys = extension_web_request_api_constants;

namespace {

// Takes |dictionary| of <string, list of strings> pairs, and gets the list
// for |key|, creating it if necessary.
base::Value* GetOrCreateList(base::DictionaryValue* dictionary,
                             const std::string& key) {
  base::Value* list = dictionary->FindKeyOfType(key, base::Value::Type::LIST);
  if (list)
    return list;
  return dictionary->SetKey(key, base::Value(base::Value::Type::LIST));
}

}  // namespace

namespace extensions {

namespace subtle {

void AppendKeyValuePair(const char* key,
                        std::unique_ptr<base::Value> value,
                        base::ListValue* list) {
  std::unique_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue);
  dictionary->SetWithoutPathExpansion(key, std::move(value));
  list->Append(std::move(dictionary));
}

}  // namespace subtle

UploadDataPresenter::~UploadDataPresenter() {}

RawDataPresenter::RawDataPresenter()
  : success_(true),
    list_(new base::ListValue) {
}
RawDataPresenter::~RawDataPresenter() {}

void RawDataPresenter::FeedBytes(base::StringPiece bytes) {
  if (!success_)
    return;

  FeedNextBytes(bytes.data(), bytes.size());
}

void RawDataPresenter::FeedFile(const base::FilePath& path) {
  if (!success_)
    return;

  FeedNextFile(path.AsUTF8Unsafe());
}

bool RawDataPresenter::Succeeded() {
  return success_;
}

std::unique_ptr<base::Value> RawDataPresenter::Result() {
  if (!success_)
    return nullptr;

  return std::move(list_);
}

void RawDataPresenter::FeedNextBytes(const char* bytes, size_t size) {
  subtle::AppendKeyValuePair(keys::kRequestBodyRawBytesKey,
                             Value::CreateWithCopiedBuffer(bytes, size),
                             list_.get());
}

void RawDataPresenter::FeedNextFile(const std::string& filename) {
  // Insert the file path instead of the contents, which may be too large.
  subtle::AppendKeyValuePair(keys::kRequestBodyRawFileKey,
                             std::make_unique<base::Value>(filename),
                             list_.get());
}

ParsedDataPresenter::ParsedDataPresenter(
    const net::HttpRequestHeaders& request_headers)
    : parser_(FormDataParser::Create(request_headers)),
      success_(parser_ != nullptr),
      dictionary_(success_ ? new base::DictionaryValue() : nullptr) {}

ParsedDataPresenter::~ParsedDataPresenter() {}

void ParsedDataPresenter::FeedBytes(base::StringPiece bytes) {
  if (!success_)
    return;

  if (!parser_->SetSource(bytes)) {
    Abort();
    return;
  }

  FormDataParser::Result result;
  while (parser_->GetNextNameValue(&result)) {
    base::Value* list = GetOrCreateList(dictionary_.get(), result.name());
    list->Append(result.take_value());
  }
}

void ParsedDataPresenter::FeedFile(const base::FilePath& path) {}

bool ParsedDataPresenter::Succeeded() {
  if (success_ && !parser_->AllDataReadOK())
    Abort();
  return success_;
}

std::unique_ptr<base::Value> ParsedDataPresenter::Result() {
  if (!success_)
    return nullptr;

  return std::move(dictionary_);
}

// static
std::unique_ptr<ParsedDataPresenter> ParsedDataPresenter::CreateForTests() {
  static const std::string form_type("application/x-www-form-urlencoded");
  return base::WrapUnique(new ParsedDataPresenter(form_type));
}

ParsedDataPresenter::ParsedDataPresenter(const std::string& form_type)
  : parser_(FormDataParser::CreateFromContentTypeHeader(&form_type)),
    success_(parser_.get() != NULL),
    dictionary_(success_ ? new base::DictionaryValue() : NULL) {
}

void ParsedDataPresenter::Abort() {
  success_ = false;
  dictionary_.reset();
  parser_.reset();
}

}  // namespace extensions
