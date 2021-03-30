// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/dictionary_value_update.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/strings/string_split.h"
#include "base/values.h"

namespace prefs {

DictionaryValueUpdate::DictionaryValueUpdate(UpdateCallback report_update,
                                             base::DictionaryValue* value,
                                             std::vector<std::string> path)
    : report_update_(std::move(report_update)),
      value_(value),
      path_(std::move(path)) {}

DictionaryValueUpdate::~DictionaryValueUpdate() = default;

bool DictionaryValueUpdate::HasKey(base::StringPiece key) const {
  return value_->HasKey(key);
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
  value_->Clear();
}

void DictionaryValueUpdate::Set(base::StringPiece path,
                                std::unique_ptr<base::Value> in_value) {
  const base::Value* old_value = nullptr;
  if (value_->Get(path, &old_value) && *old_value == *in_value)
    return;

  RecordPath(path);
  value_->Set(path, std::move(in_value));
}

void DictionaryValueUpdate::SetPath(
    std::initializer_list<base::StringPiece> path,
    base::Value value) {
  const base::Value* found = value_->FindPath(path);
  if (found && *found == value)
    return;

  RecordSplitPath(path);
  value_->SetPath(path, std::move(value));
}

void DictionaryValueUpdate::SetBoolean(base::StringPiece path, bool in_value) {
  Set(path, std::make_unique<base::Value>(in_value));
}

void DictionaryValueUpdate::SetInteger(base::StringPiece path, int in_value) {
  Set(path, std::make_unique<base::Value>(in_value));
}

void DictionaryValueUpdate::SetDouble(base::StringPiece path, double in_value) {
  Set(path, std::make_unique<base::Value>(in_value));
}

void DictionaryValueUpdate::SetString(base::StringPiece path,
                                      base::StringPiece in_value) {
  Set(path, std::make_unique<base::Value>(in_value));
}

void DictionaryValueUpdate::SetString(base::StringPiece path,
                                      const std::u16string& in_value) {
  Set(path, std::make_unique<base::Value>(in_value));
}

std::unique_ptr<DictionaryValueUpdate> DictionaryValueUpdate::SetDictionary(
    base::StringPiece path,
    std::unique_ptr<base::DictionaryValue> in_value) {
  RecordPath(path);
  base::DictionaryValue* dictionary_value =
      value_->SetDictionary(path, std::move(in_value));

  return std::make_unique<DictionaryValueUpdate>(
      report_update_, dictionary_value, ConcatPath(path_, path));
}

void DictionaryValueUpdate::SetKey(base::StringPiece key, base::Value value) {
  const base::Value* found = value_->FindKey(key);
  if (found && *found == value)
    return;

  RecordKey(key);
  value_->SetKey(key, std::move(value));
}

void DictionaryValueUpdate::SetWithoutPathExpansion(
    base::StringPiece key,
    std::unique_ptr<base::Value> in_value) {
  const base::Value* old_value = nullptr;
  if (value_->GetWithoutPathExpansion(key, &old_value) &&
      *old_value == *in_value) {
    return;
  }
  RecordKey(key);
  value_->SetWithoutPathExpansion(key, std::move(in_value));
}

std::unique_ptr<DictionaryValueUpdate>
DictionaryValueUpdate::SetDictionaryWithoutPathExpansion(
    base::StringPiece path,
    std::unique_ptr<base::DictionaryValue> in_value) {
  RecordKey(path);
  auto* dictionary_value = static_cast<base::DictionaryValue*>(
      value_->SetWithoutPathExpansion(path, std::move(in_value)));

  std::vector<std::string> full_path = path_;
  full_path.push_back(path.as_string());
  return std::make_unique<DictionaryValueUpdate>(
      report_update_, dictionary_value, std::move(full_path));
}

bool DictionaryValueUpdate::GetBoolean(base::StringPiece path,
                                       bool* out_value) const {
  return value_->GetBoolean(path, out_value);
}

bool DictionaryValueUpdate::GetInteger(base::StringPiece path,
                                       int* out_value) const {
  return value_->GetInteger(path, out_value);
}

bool DictionaryValueUpdate::GetDouble(base::StringPiece path,
                                      double* out_value) const {
  return value_->GetDouble(path, out_value);
}

bool DictionaryValueUpdate::GetString(base::StringPiece path,
                                      std::string* out_value) const {
  return value_->GetString(path, out_value);
}

bool DictionaryValueUpdate::GetString(base::StringPiece path,
                                      std::u16string* out_value) const {
  return value_->GetString(path, out_value);
}

bool DictionaryValueUpdate::GetDictionary(
    base::StringPiece path,
    const base::DictionaryValue** out_value) const {
  return AsConstDictionary()->GetDictionary(path, out_value);
}

bool DictionaryValueUpdate::GetDictionary(
    base::StringPiece path,
    std::unique_ptr<DictionaryValueUpdate>* out_value) {
  base::DictionaryValue* dictionary_value = nullptr;
  if (!value_->GetDictionary(path, &dictionary_value))
    return false;

  *out_value = std::make_unique<DictionaryValueUpdate>(
      report_update_, dictionary_value, ConcatPath(path_, path));
  return true;
}

bool DictionaryValueUpdate::GetList(base::StringPiece path,
                                    const base::ListValue** out_value) const {
  return AsConstDictionary()->GetList(path, out_value);
}

bool DictionaryValueUpdate::GetList(base::StringPiece path,
                                    base::ListValue** out_value) {
  RecordPath(path);
  return value_->GetList(path, out_value);
}

bool DictionaryValueUpdate::GetBooleanWithoutPathExpansion(
    base::StringPiece key,
    bool* out_value) const {
  return value_->GetBooleanWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::GetIntegerWithoutPathExpansion(
    base::StringPiece key,
    int* out_value) const {
  return value_->GetIntegerWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::GetDoubleWithoutPathExpansion(
    base::StringPiece key,
    double* out_value) const {
  return value_->GetDoubleWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::GetStringWithoutPathExpansion(
    base::StringPiece key,
    std::string* out_value) const {
  return value_->GetStringWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::GetStringWithoutPathExpansion(
    base::StringPiece key,
    std::u16string* out_value) const {
  return value_->GetStringWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::GetDictionaryWithoutPathExpansion(
    base::StringPiece key,
    const base::DictionaryValue** out_value) const {
  return value_->GetDictionaryWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::GetDictionaryWithoutPathExpansion(
    base::StringPiece key,
    std::unique_ptr<DictionaryValueUpdate>* out_value) {
  base::DictionaryValue* dictionary_value = nullptr;
  if (!value_->GetDictionaryWithoutPathExpansion(key, &dictionary_value))
    return false;

  std::vector<std::string> full_path = path_;
  full_path.push_back(key.as_string());
  *out_value = std::make_unique<DictionaryValueUpdate>(
      report_update_, dictionary_value, std::move(full_path));
  return true;
}

bool DictionaryValueUpdate::GetListWithoutPathExpansion(
    base::StringPiece key,
    const base::ListValue** out_value) const {
  return value_->GetListWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::GetListWithoutPathExpansion(
    base::StringPiece key,
    base::ListValue** out_value) {
  RecordKey(key);
  return value_->GetListWithoutPathExpansion(key, out_value);
}

bool DictionaryValueUpdate::Remove(base::StringPiece path,
                                   std::unique_ptr<base::Value>* out_value) {
  if (!value_->Remove(path, out_value))
    return false;

  RecordPath(path);
  return true;
}

bool DictionaryValueUpdate::RemoveWithoutPathExpansion(
    base::StringPiece key,
    std::unique_ptr<base::Value>* out_value) {
  if (!value_->RemoveWithoutPathExpansion(key, out_value))
    return false;

  RecordKey(key);
  return true;
}

bool DictionaryValueUpdate::RemovePath(
    base::StringPiece path,
    std::unique_ptr<base::Value>* out_value) {
  if (!value_->RemovePath(path, out_value))
    return false;

  std::vector<base::StringPiece> split_path = SplitPath(path);
  base::DictionaryValue* dict = value_;
  for (size_t i = 0; i < split_path.size() - 1; ++i) {
    if (!dict->GetDictionary(split_path[i], &dict)) {
      split_path.resize(i + 1);
      break;
    }
  }
  RecordSplitPath(split_path);
  return true;
}

base::DictionaryValue* DictionaryValueUpdate::AsDictionary() {
  RecordSplitPath(std::vector<base::StringPiece>());
  return value_;
}

const base::DictionaryValue* DictionaryValueUpdate::AsConstDictionary() const {
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
                 [](base::StringPiece s) { return s.as_string(); });
  return full_path;
}

}  // namespace prefs
