// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_TRAITS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_TRAITS_H_

// Controls which profile to use when the factory is asked to create a
// service for an off-the-record profile.
enum class ProfileSelection {
  kNoInstanceInIncognito,
  kRedirectedInIncognito,
  kOwnInstanceInIncognito,
  kDefault = kNoInstanceInIncognito,
};

// Controls when the service should be created.
enum class ServiceCreation {
  kCreateLazily,
  kCreateWithProfile,
  kDefault = kCreateLazily,
};

// Controls whether the service should be created for test Profiles.
enum class TestingCreation {
  kCreateService,
  kNoServiceForTests,
  kDefault = kCreateService,
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_TRAITS_H_
