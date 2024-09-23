// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/fake_suggestions_database.h"

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

namespace {

std::map<std::u16string, std::string> DeserializeJSON(const std::string& str) {
  JSONStringValueDeserializer deserializer(str,
                                           base::JSON_ALLOW_TRAILING_COMMAS);
  int error_code = 0;
  std::string error_message = "";
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(&error_code, &error_message);

  DCHECK(!error_code) << error_message;

  // The root should be a list containing the suggestions.
  const base::Value::List& list = root->GetList();
  auto fake_suggestions = std::map<std::u16string, std::string>();
  for (size_t i = 0; i < list.size(); ++i) {
    // A suggest response should be in the list format with the search terms in
    // front.
    const base::Value::List& response = list[i].GetList();
    const std::string& search_terms = response.front().GetString();
    std::string serialized_response = "";
    JSONStringValueSerializer(&serialized_response).Serialize(response);
    fake_suggestions.insert(
        std::make_pair(base::UTF8ToUTF16(search_terms), serialized_response));
  }
  return fake_suggestions;
}

}  // namespace

FakeSuggestionsDatabase::FakeSuggestionsDatabase(
    TemplateURLService* template_url_service)
    : template_url_service_(template_url_service), data_() {}

FakeSuggestionsDatabase::~FakeSuggestionsDatabase() = default;

void FakeSuggestionsDatabase::LoadSuggestionsFromFile(
    const base::FilePath& file_path) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&FakeSuggestionsDatabase::LoadFakeSuggestions,
                     base::Unretained(this), file_path));
}

bool FakeSuggestionsDatabase::HasFakeSuggestions(const GURL& url) const {
  std::u16string search_terms = ExtractSearchTerms(url);
  return base::Contains(data_, search_terms);
}

std::string FakeSuggestionsDatabase::GetFakeSuggestions(const GURL& url) const {
  std::u16string search_terms = ExtractSearchTerms(url);
  auto item = data_.find(search_terms);
  if (item != data_.end()) {
    return item->second;
  } else {
    return "";
  }
}

void FakeSuggestionsDatabase::SetFakeSuggestions(
    const GURL& url,
    const std::string& fake_suggestions) {
  std::u16string search_terms = ExtractSearchTerms(url);
  data_[search_terms] = fake_suggestions;
}

std::u16string FakeSuggestionsDatabase::ExtractSearchTerms(
    const GURL& url) const {
  DCHECK(template_url_service_ != nullptr);
  if (!template_url_service_ &&
      template_url_service_->GetDefaultSearchProvider()) {
    return u"";
  }
  const TemplateURLRef& suggestion_url_ref =
      template_url_service_->GetDefaultSearchProvider()->suggestions_url_ref();
  std::u16string search_terms;
  url::Parsed::ComponentType search_term_component;
  url::Component search_terms_position;
  suggestion_url_ref.ExtractSearchTermsFromURL(
      url, &search_terms, template_url_service_->search_terms_data(),
      &search_term_component, &search_terms_position);
  return search_terms;
}

void FakeSuggestionsDatabase::LoadFakeSuggestions(base::FilePath file_path) {
  if (!base::PathExists(file_path)) {
    NOTREACHED_IN_MIGRATION() << "File doesn't exist";
  }
  std::string data_str = "";
  if (base::ReadFileToString(file_path, &data_str)) {
    std::map<std::u16string, std::string> file_data = DeserializeJSON(data_str);
    data_.merge(file_data);
  }
}
