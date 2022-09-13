// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GOOGLE_API_KEYS_UNITTEST_H_
#define GOOGLE_APIS_GOOGLE_API_KEYS_UNITTEST_H_

#include <memory>
#include <string>

#include "base/environment.h"
#include "google_apis/google_api_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

struct EnvironmentCache {
  EnvironmentCache() : variable_name(nullptr), was_set(false) {}

  const char* variable_name;
  bool was_set;
  std::string value;
};

class GoogleAPIKeysTest : public testing::Test {
 public:
  GoogleAPIKeysTest();
  ~GoogleAPIKeysTest() override;
  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<base::Environment> env_;

  // Why 3?  It is for GOOGLE_API_KEY, GOOGLE_DEFAULT_CLIENT_ID and
  // GOOGLE_DEFAULT_CLIENT_SECRET.
  //
  // Why 2 times CLIENT_NUM_ITEMS?  This is the number of different
  // clients in the OAuth2Client enumeration, and for each of these we
  // have both an ID and a secret.
  EnvironmentCache env_cache_[3 + 2 * google_apis::CLIENT_NUM_ITEMS];
};

#endif  // GOOGLE_APIS_GOOGLE_API_KEYS_UNITTEST_H_
