// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_LAZY_UNEXPORTABLE_PRIVATE_KEY_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_LAZY_UNEXPORTABLE_PRIVATE_KEY_FACTORY_H_

#import <memory>
#import <string>
#import <vector>

#import "base/functional/callback_forward.h"
#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "components/enterprise/client_certificates/core/private_key_factory.h"

namespace client_certificates {

// A PrivateKeyFactory implementation that defers the creation of its
// underlying `UnexportablePrivateKeyFactory` to a background thread. This is
// necessary because resolving the keychain access group on iOS involves
// blocking calls and causes hangs during startup.
class LazyUnexportablePrivateKeyFactory : public PrivateKeyFactory {
 public:
  explicit LazyUnexportablePrivateKeyFactory(std::string_view application_tag);

  ~LazyUnexportablePrivateKeyFactory() override;

  void CreatePrivateKey(PrivateKeyCallback callback) override;
  void LoadPrivateKey(
      const client_certificates_pb::PrivateKey& serialized_private_key,
      PrivateKeyCallback callback) override;
  void LoadPrivateKeyFromDict(const base::DictValue& serialized_private_key,
                              PrivateKeyCallback callback) override;

  static void SetFactoryCreatorForTesting(
      std::unique_ptr<PrivateKeyFactory> (*func)());

 private:
  static std::unique_ptr<PrivateKeyFactory> CreateBaseFactory(
      std::string application_tag);
  void OnBaseFactoryCreated(std::unique_ptr<PrivateKeyFactory> factory);

  std::string application_tag_;
  std::unique_ptr<PrivateKeyFactory> base_factory_;
  std::vector<base::OnceClosure> pending_operations_;

  base::WeakPtrFactory<LazyUnexportablePrivateKeyFactory> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_LAZY_UNEXPORTABLE_PRIVATE_KEY_FACTORY_H_
