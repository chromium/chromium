// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/search_engines_util.h"

#include <stddef.h>

#include <memory>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

namespace {

// Id of the google id in template_url_prepopulate_data.cc.
static const int kGoogleEnginePrepopulatedId = 1;

// Update the search engine of the given service to the default ones for the
// current locale.
void UpdateSearchEngine(TemplateURLService* service) {
  DCHECK(service);
  DCHECK(service->loaded());
  std::vector<TemplateURL*> old_engines = service->GetTemplateURLs();
  size_t default_engine_index;
  std::vector<std::unique_ptr<TemplateURLData>> new_engines =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(nullptr,
                                                         &default_engine_index);
  DCHECK(default_engine_index == 0);
  DCHECK(new_engines[0]->prepopulate_id == kGoogleEnginePrepopulatedId);
  // The aim is to replace the old search engines with the new ones.
  // It is not possible to remove all of them, because removing the current
  // selected engine is not allowed.
  // It is not possible to add all the new ones first, because the service gets
  // confused when a prepopulated engine is there more than once.
  // Instead, this will in a first pass make Google as the default engine if
  // the currently selected engine is neither Google nor custom engine. Then it
  // will remove all prepopulated search engines except Google. At last, in a
  // third pass, it will add all new engines except Google.
  const TemplateURL* default_engine = service->GetDefaultSearchProvider();
  if (default_engine->prepopulate_id() > kGoogleEnginePrepopulatedId) {
    for (auto* engine : old_engines) {
      if (engine->prepopulate_id() == kGoogleEnginePrepopulatedId) {
        service->SetUserSelectedDefaultSearchProvider(engine);
        break;
      }
    }
  }
  for (auto* engine : old_engines) {
    if (engine->prepopulate_id() > kGoogleEnginePrepopulatedId)
      service->Remove(engine);
  }
  for (const auto& engine : new_engines) {
    if (engine->prepopulate_id != kGoogleEnginePrepopulatedId)
      service->Add(std::make_unique<TemplateURL>(*engine));
  }
}

// Observer class that allows to wait for the TemplateURLService to be loaded.
// This class will delete itself as soon as the TemplateURLService is loaded.
class LoadedObserver : public TemplateURLServiceObserver {
 public:
  explicit LoadedObserver(TemplateURLService* service) : service_(service) {
    DCHECK(service_);
    DCHECK(!service_->loaded());
    service_->AddObserver(this);
    service_->Load();
  }

  ~LoadedObserver() override { service_->RemoveObserver(this); }

  void OnTemplateURLServiceChanged() override {
    service_->RemoveObserver(this);
    UpdateSearchEngine(service_);
    // Only delete this class when this callback is finished.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  TemplateURLService* service_;
};

}  // namespace

namespace search_engines {

void UpdateSearchEnginesIfNeeded(PrefService* preferences,
                                 TemplateURLService* service) {
  if (!preferences->HasPrefPath(country_codes::kCountryIDAtInstall)) {
    // No search engines were ever installed, just return.
    return;
  }
  int old_country_id =
      preferences->GetInteger(country_codes::kCountryIDAtInstall);
  int country_id = country_codes::GetCurrentCountryID();
  if (country_id == old_country_id) {
    // User's locale did not change, just return.
    return;
  }
  preferences->SetInteger(country_codes::kCountryIDAtInstall, country_id);
  // If the current search engine is managed by policy then we can't set the
  // default search engine, which is required by UpdateSearchEngine(). This
  // isn't a problem as long as the default search engine is enforced via
  // policy, because it can't be changed by the user anyway; if the policy is
  // removed then the engines can be updated again.
  if (!service || service->is_default_search_managed())
    return;
  if (service->loaded())
    UpdateSearchEngine(service);
  else
    new LoadedObserver(service);  // The observer manages its own lifetime.
}

bool SupportsSearchByImage(TemplateURLService* service) {
  if (!service) {
    return false;
  }
  const TemplateURL* default_url = service->GetDefaultSearchProvider();
  return default_url && !default_url->image_url().empty() &&
         default_url->image_url_ref().IsValid(service->search_terms_data());
}

}  // namespace search_engines
