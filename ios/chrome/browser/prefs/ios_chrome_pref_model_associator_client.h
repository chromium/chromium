// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string>

#include "base/no_destructor.h"
#include "components/sync_preferences/pref_model_associator_client.h"

class IOSChromePrefModelAssociatorClient
    : public sync_preferences::PrefModelAssociatorClient {
 public:
  // Returns the global instance.
  static IOSChromePrefModelAssociatorClient* GetInstance();

  IOSChromePrefModelAssociatorClient(
      const IOSChromePrefModelAssociatorClient&) = delete;
  IOSChromePrefModelAssociatorClient& operator=(
      const IOSChromePrefModelAssociatorClient&) = delete;

 private:
  friend class base::NoDestructor<IOSChromePrefModelAssociatorClient>;

  IOSChromePrefModelAssociatorClient();
  ~IOSChromePrefModelAssociatorClient() override;

  // sync_preferences::PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override;
  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override;
  base::Value MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override;
};

#endif  // IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
