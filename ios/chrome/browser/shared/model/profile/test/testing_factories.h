// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TESTING_FACTORIES_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TESTING_FACTORIES_H_

#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"

class ProfileIOS;

// Type of the callbacks that can be registered as a testing factory
// for a ProfileKeyedServiceFactoryIOS sub-class.
using KeyedServiceTestingFactoryIOS =
    base::OnceCallback<std::unique_ptr<KeyedService>(ProfileIOS&)>;

// Type of the callbacks that can be registered as a testing factory
// for a RefcountedKeyedServiceFactoryIOS sub-class.
using RefcountedKeyedServiceTestingFactoryIOS =
    base::OnceCallback<scoped_refptr<RefcountedKeyedService>(ProfileIOS&)>;

namespace ios::test::internal {

// Adapts `factory` to conforms to {Refcounted,}KeyedServiceTestingFactoryIOS.
//
// Converts the return type of the callback from `SmartPtr<Type>` into
// `SmartPtr<Base>` (thus allowing to bind callback returning a sub-class
// without expliciting the returng type).
template <typename Type,
          template <class> class SmartPtr,
          typename Base = std::conditional_t<base::IsRefCountedType<Type>,
                                             RefcountedKeyedService,
                                             KeyedService>>
  requires std::convertible_to<Type*, Base*>
base::OnceCallback<SmartPtr<Base>(ProfileIOS&)> AdaptTestingFactory(
    base::OnceCallback<SmartPtr<Type>(ProfileIOS&)> factory) {
  if constexpr (std::is_same_v<Type, Base>) {
    return factory;
  }

  if (!factory) {
    return {};
  }

  return base::BindOnce(
      [](base::OnceCallback<SmartPtr<Type>(ProfileIOS&)> factory,
         ProfileIOS& profile) -> SmartPtr<Base> {
        return std::move(factory).Run(profile);
      },
      std::move(factory));
}

// Adapts `factory` to conforms to {Refcounted,}KeyedServiceTestingFactoryIOS.
//
// Converts callback with no parameters into a callback taking `ProfileIOS&`
// (thus allowing to bind `&std::make_unique<FakeService>`).
template <typename Type,
          template <class> class SmartPtr,
          typename Base = std::conditional_t<base::IsRefCountedType<Type>,
                                             RefcountedKeyedService,
                                             KeyedService>>
  requires std::convertible_to<Type*, Base*>
base::OnceCallback<SmartPtr<Base>(ProfileIOS&)> AdaptTestingFactory(
    base::OnceCallback<SmartPtr<Type>()> factory) {
  if (!factory) {
    return {};
  }

  return AdaptTestingFactory(base::IgnoreArgs<ProfileIOS&>(std::move(factory)));
}

// Adapts `factory` to conforms to {Refcounted,}KeyedServiceTestingFactoryIOS.
//
// Converts callback taking `ProfileIOS*` into a callback taking `ProfileIOS&`
// (thus allowing incremental migration of the tests from the old API).
//
// TODO(crbug.com/576182342): Remove once all tests have been converted to use
// factories that accept a ProfileIOS& or no-parameters.
template <typename Type,
          template <class> class SmartPtr,
          typename Base = std::conditional_t<base::IsRefCountedType<Type>,
                                             RefcountedKeyedService,
                                             KeyedService>>
  requires std::convertible_to<Type*, Base*>
base::OnceCallback<SmartPtr<Base>(ProfileIOS&)> AdaptTestingFactory(
    base::OnceCallback<SmartPtr<Type>(ProfileIOS*)> factory) {
  if (!factory) {
    return {};
  }

  return base::BindOnce(
      [](base::OnceCallback<SmartPtr<Type>(ProfileIOS*)> factory,
         ProfileIOS& profile) -> SmartPtr<Base> {
        return std::move(factory).Run(&profile);
      },
      std::move(factory));
}

// Adapts `factory` to conforms to {Refcounted,}KeyedServiceTestingFactoryIOS.
//
// Converts a repeating callback into a once callback (as implicit conversions
// are ignored when performing type deductions in template instantiations).
//
// TODO(crbug.com/576182342): Remove once all tests have been converted to use
// base::BindOnce(...) to create their factories.
template <typename Type,
          typename... Args,
          template <class> class SmartPtr,
          typename Base = std::conditional_t<base::IsRefCountedType<Type>,
                                             RefcountedKeyedService,
                                             KeyedService>>
  requires std::convertible_to<Type*, Base*>
base::OnceCallback<SmartPtr<Base>(ProfileIOS&)> AdaptTestingFactory(
    base::RepeatingCallback<SmartPtr<Type>(Args...)> factory) {
  return AdaptTestingFactory(
      base::OnceCallback<SmartPtr<Type>(Args...)>(std::move(factory)));
}

}  // namespace ios::test::internal

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TEST_TESTING_FACTORIES_H_
