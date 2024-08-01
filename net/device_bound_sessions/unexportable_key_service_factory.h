// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_
#define NET_DEVICE_BOUND_SESSIONS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "net/base/net_export.h"

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace net::device_bound_sessions {

class NET_EXPORT UnexportableKeyServiceFactory {
 public:
  ~UnexportableKeyServiceFactory();
  // Returns nullptr if unexportable key provider is not supported by the
  // platform or the device.
  // It should consistently return nullptr or not while chrome is running,
  // and most likely on the same device/os combo over time.
  unexportable_keys::UnexportableKeyService* GetShared();

  static UnexportableKeyServiceFactory* GetInstance();

  UnexportableKeyServiceFactory(const UnexportableKeyServiceFactory&) = delete;
  UnexportableKeyServiceFactory& operator=(
      const UnexportableKeyServiceFactory&) = delete;

  void SetUnexportableKeyFactoryForTesting(
      unexportable_keys::UnexportableKeyService* (*func)());

  static UnexportableKeyServiceFactory* GetInstanceForTesting();

 private:
  friend class base::NoDestructor<UnexportableKeyServiceFactory>;
  std::unique_ptr<unexportable_keys::UnexportableKeyService>
      unexportable_key_service_;
  bool has_created_service_ = false;

  UnexportableKeyServiceFactory();
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_UNEXPORTABLE_KEY_SERVICE_FACTORY_H_
