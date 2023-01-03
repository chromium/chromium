// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/dictionary_value_update.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace prefs {

DictionaryValueUpdate::DictionaryValueUpdate(UpdateCallback report_update,
                                             base::Value::Dict* value,
                                             std::vector<std::string> path)
    : report_update_(std::move(report_update)),
      value_(value),
      path_(std::move(path)) {
  DCHECK(value_);
}

DictionaryValueUpdate::~DictionaryValueUpdate() = default;

bool DictionaryValueUpdate::HasKey(base::StringPiece key) const {
  return value_->contains(key);
}

size_t DictionaryValueUpdate::size() const {
  return value_->size();
}

bool DictionaryValueUpdate::empty() const {
  return value_->empty();
}

void DictionaryValueUpdate::Clear() {
  if (empty())
    return;

  RecordSplitPath(std::vector<base::StringPiece>());
  value_->clear();
}

void DictionaryValueUpdate::Set(base::StringPiece path, base::Value in_value) {
  const base::Value* old_value = value_->FindByDottedPath(path);
  if (old_value && *old_value == in_value)
    return;

  RecordPath(path);
  value_->SetByDottedPath(path, std::move(in_value));
}

void DictionaryValueUpdate::SetBoolean(base::StringPiece path, bool in_value) {
  Set(path, base::Value(in_value));
}

void DictionaryValueUpdate::SetInteger(base::StringPiece path, int in_value) {
  Set(path, base::Value(in_value));
}

void DictionaryValueUpdate::SetDouble(base::StringPiece path, double in_value) {
  Set(path, base::Value(in_value));
}

void DictionaryValueUpdate::SetString(base::StringPiece path,
                                      base::StringPiece in_value) {
  Set(path, base::Value(in_value));
}

void DictionaryValueUpdate::SetString(base::StringPiece path,
                                      const std::u16string& in_value) {
  Set(path, base::Value(in_value));
}

std::unique_ptr<DictionaryValueUpdate> DictionaryValueUpdate::SetDictionary(
    base::StringPiece path,
    base::Value::Dict in_value) {
  RecordPath(path);
  base::Value::Dict& dictionary_value =
      value_->SetByDottedPath(path, std::move(in_value))->GetDict();

  return std::make_unique<DictionaryValueUpdate>(
      report_update_, &dictionary_value, ConcatPath(path_, path));
}

base::Value* DictionaryValueUpdate::SetKey(base::StringPiece key,
                                           base::Value value) {
  base::Value* found = value_->Find(key);
  if (found && *found == value)
    return found;

  RecordKey(key);
  return value_->Set(key, std::move(value));
}

void DictionaryValueUpdate::SetWithoutPathExpansion(
    base::StringPiece key,
    std::unique_ptr<base::Value> in_value) {
  const base::Value* old_value = value_->Find(key);
  if (old_value && *old_value == *in_value) {
    return;
  }
  RecordKey(key);
  value_->Set(key, base::Value::FromUniquePtrValue(std::move(in_value)));
}

std::unique_ptr<DictionaryValueUpdate>
DictionaryValueUpdate::SetDictionaryWithoutPathExpansion(
    base::StringPiece path,
    base::Value::Dict in_value) {
  RecordKey(path);
  base::Value::Dict& dictionary_value =
      value_->Set(path, std::move(in_value))->GetDict();

  std::vector<std::string> full_path = path_;
  full_path.push_back(std::string(path));
  return std::make_unique<DictionaryValueUpdate>(
      report_update_, &dictionary_value, std::move(full_path));
}

bool DictionaryValueUpdate::GetBoolean(base::StringPiece path,
                                       bool* out_value) const {
  absl::optional<bool> value = value_->FindBoolByDottedPath(path);
  if (!value.has_value())
    return false;
  if (out_value)
    *out_value = *value;
  return true;
}

bool DictionaryValueUpdate::GetInteger(base::StringPiece path,
                                       int* out_value) const {
  if (absl::optional<int> value = value_->FindIntByDottedPath(path)) {
    if (out_value) {
      *out_value = *value;
    }
    return true;
  }
  return false;
}

bool DictionaryValueUpdate::GetDouble(base::StringPiece path,
                                      double* out_value) const {
  if (absl::optional<double> value = value_->FindDoubleByDottedPath(path)) {
    if (out_value) {
      *out_value = *value;
    }
    return true;
  }
  return false;
}

bool DictionaryValueUpdate::GetString(base::StringPiece path,
                                      std::string* out_value) const {
  if (std::string* value = value_->FindStringByDottedPath(path)) {
    if (out_value) {
      *out_value = *value;
    }
    return true;
  }
  return false;
}

bool DictionaryValueUpdate::GetDictionary(
    base::StringPiece path,
    const base::Value::Dict** out_value) const {
  const base::Value::Dict* dict = value_->FindDictByDottedPath(path);
  if (!dict) {
    return false;
  }
  if (out_value) {
    *out_value = dict;
  }

  return true;
}

bool DictionaryValueUpdate::GetDictionary(
    base::StringPiece path,
    std::unique_ptr<DictionaryValueUpdate>* out_value) {
  base::Value::Dict* dict = value_->FindDictByDottedPath(path);
  if (!dict) {
    return false;
  }
  if (out_value) {
    *out_value = std::make_unique<DictionaryValueUpdate>(
        report_update_, dict, ConcatPath(path_, path));
  }
  return true;
}

bool DictionaryValueUpdate::GetBooleanWithoutPathExpansion(
    base::StringPiece key,
    bool* out_value) const {
  absl::optional<bool> flag = value_->FindBool(key);
  if (!flag)
    return false;

  *out_value = flag.value();
  return true;
}

bool DictionaryValueUpdate::GetIntegerWithoutPathExpansion(
    base::StringPiece key,
    int* out_value) const {
  absl::optional<int> value = value_->FindInt(key);
  if (!value)
    return false;

  *out_value = value.value();
  return true;
}

bool DictionaryValueUpdate::GetDoubleWithoutPathExpansion(
    base::StringPiece key,
    double* out_value) const {
  absl::optional<double> value = value_->FindDouble(key);
  if (!value)
    return false;

  *out_value = value.value();
  return true;
}

bool DictionaryValueUpdate::GetStringWithoutPathExpansion(
    base::StringPiece key,
    std::string* out_value) const {
  std::string* value = value_->FindString(key);
  if (!value)
    return false;

  *out_value = *value;
  return true;
}

bool DictionaryValueUpdate::GetStringWithoutPathExpansion(
    base::StringPiece key,
    std::u16string* out_value) const {
  std::string* value = value_->FindString(key);
  if (!value)
    return false;

  *out_value = base::UTF8ToUTF16(*value);
  return true;
}

bool DictionaryValueUpdate::GetDictionaryWithoutPathExpansion(
    base::StringPiece key,
    const base::Value::Dict** out_value) const {
  const base::Value::Dict* value = value_->FindDict(key);
  if (!value) {
    return false;
  }
  if (out_value)
    *out_value = value;
  return true;
}

bool DictionaryValueUpdate::GetDictionaryWithoutPathExpansion(
    base::StringPiece key,
    std::unique_ptr<DictionaryValueUpdate>* out_value) {
  base::Value::Dict* dictionary_value = nullptr;
  if (!std::as_const(*this).GetDictionaryWithoutPathExpansion(
          key, const_cast<const base::Value::Dict**>(&dictionary_value))) {
    return false;
  }

  std::vector<std::string> full_path = path_;
  full_path.push_back(std::string(key));
  *out_value = std::make_unique<DictionaryValueUpdate>(
      report_update_, dictionary_value, std::move(full_path));
  return true;
}

bool DictionaryValueUpdate::GetListWithoutPathExpansion(
    base::StringPiece key,
    const base::Value::List** out_value) const {
  const base::Value::List* list = value_->FindList(key);
  if (!list)
    return false;
  if (out_value)
    *out_value = list;
  return true;
}

bool DictionaryValueUpdate::GetListWithoutPathExpansion(
    base::StringPiece key,
    base::Value::List** out_value) {
  RecordKey(key);
  return std::as_const(*this).GetListWithoutPathExpansion(
      key, const_cast<const base::Value::List**>(out_value));
}

bool DictionaryValueUpdate::Remove(base::StringPiece path) {
  base::StringPiece current_path(path);
  base::Value::Dict* current_dictionary = value_;
  size_t delimiter_position = current_path.rfind('.');
  if (delimiter_position != base::StringPiece::npos) {
    current_dictionary = value_->FindDictByDottedPath(
        current_path.substr(0, delimiter_position));
    if (!current_dictionary)
      return false;
    current_path = current_path.substr(delimiter_position + 1);
  }
  if (!current_dictionary->Remove(current_path)) {
    return false;
  }

  RecordPath(path);
  return true;
}

bool DictionaryValueUpdate::RemoveWithoutPathExpansion(
    base::StringPiece key,
    std::unique_ptr<base::Value>* out_value) {
  absl::optional<base::Value> value = value_->Extract(key);
  if (!value)
    return false;

  if (out_value)
    *out_value = base::Value::ToUniquePtrValue(std::move(*value));
  RecordKey(key);
  return true;
}

bool DictionaryValueUpdate::RemovePath(
    base::StringPiece path,
    std::unique_ptr<base::Value>* out_value) {
  absl::optional<base::Value> value = value_->ExtractByDottedPath(path);
  if (!value)
    return false;

  if (out_value)
    *out_value = base::Value::ToUniquePtrValue(std::move(*value));
  std::vector<base::StringPiece> split_path = SplitPath(path);
  base::Value::Dict* dict = value_;
  for (size_t i = 0; i < split_path.size() - 1; ++i) {
    dict = dict->FindDict(split_path[i]);
    if (!dict) {
      split_path.resize(i + 1);
      break;
    }
  }
  RecordSplitPath(split_path);
  return true;
}

base::Value::Dict* DictionaryValueUpdate::AsDict() {
  RecordSplitPath(std::vector<base::StringPiece>());
  return value_;
}

const base::Value::Dict* DictionaryValueUpdate::AsConstDict() const {
  return value_;
}

void DictionaryValueUpdate::RecordKey(base::StringPiece key) {
  RecordSplitPath({key});
}

void DictionaryValueUpdate::RecordPath(base::StringPiece path) {
  RecordSplitPath(SplitPath(path));
}

void DictionaryValueUpdate::RecordSplitPath(
    const std::vector<base::StringPiece>& path) {
  report_update_.Run(ConcatPath(path_, path));
}

std::vector<base::StringPiece> DictionaryValueUpdate::SplitPath(
    base::StringPiece path) {
  return base::SplitStringPiece(path, ".", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

std::vector<std::string> DictionaryValueUpdate::ConcatPath(
    const std::vector<std::string>& base_path,
    base::StringPiece path) {
  return ConcatPath(base_path, SplitPath(path));
}

std::vector<std::string> DictionaryValueUpdate::ConcatPath(
    const std::vector<std::string>& base_path,
    const std::vector<base::StringPiece>& path) {
  std::vector<std::string> full_path = base_path;
  full_path.reserve(full_path.size() + path.size());
  std::transform(path.begin(), path.end(), std::back_inserter(full_path),
                 [](base::StringPiece s) { return std::string(s); });
  return full_path;
}

}  // namespace prefs
