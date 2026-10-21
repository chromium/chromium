// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TESTING_FACTORIES_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TESTING_FACTORIES_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"

class ProfileIOS;

// Type of the callbacks that can be registered as a testing factory
// for a ProfileKeyedServiceFactoryIOS sub-class.
using KeyedServiceTestingFactoryIOS =
    base::OnceCallback<std::unique_ptr<KeyedService>(ProfileIOS*)>;

// Type of the callbacks that can be registered as a testing factory
// for a RefcountedKeyedServiceFactoryIOS sub-class.
using RefcountedKeyedServiceTestingFactoryIOS =
    base::OnceCallback<scoped_refptr<RefcountedKeyedService>(ProfileIOS*)>;

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TESTING_FACTORIES_H_
