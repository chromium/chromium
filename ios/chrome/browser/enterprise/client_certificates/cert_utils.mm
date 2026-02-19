// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/cert_utils.h"

#import <Security/Security.h>

#import <optional>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/logging.h"
#import "base/strings/strcat.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "components/enterprise/client_certificates/core/browser_cloud_management_delegate.h"
#import "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#import "components/enterprise/client_certificates/core/dm_server_client.h"
#import "components/enterprise/client_certificates/core/key_upload_client.h"
#import "components/enterprise/client_certificates/core/private_key_factory.h"
#import "components/enterprise/client_certificates/core/private_key_types.h"
#import "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"
#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "crypto/unexportable_key.h"
#import "ios/chrome/browser/enterprise/client_certificates/browser_context_delegate_ios.h"
#import "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace client_certificates {

namespace {

std::optional<std::string> ComputeAppIdentifierPrefix() {
  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrAccount) :
        base::SysUTF8ToNSString("GetAppIdentifierPrefix"),
    CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(
        "com.google.devicetrust.GetAppIdentifierPrefix"),
    CFToNSPtrCast(kSecAttrAccessible) :
        CFToNSPtrCast(kSecAttrAccessibleAfterFirstUnlock),
    CFToNSPtrCast(kSecReturnAttributes) : @YES
  };

  base::apple::ScopedCFTypeRef<CFDictionaryRef> result;
  OSStatus status = SecItemCopyMatching(
      NSToCFPtrCast(query),
      reinterpret_cast<CFTypeRef*>(result.InitializeInto()));
  if (status == errSecItemNotFound) {
    status = SecItemAdd(NSToCFPtrCast(query),
                        reinterpret_cast<CFTypeRef*>(result.InitializeInto()));
  }

  if (status != errSecSuccess || !result) {
    LOG(ERROR) << "Unable to get keychain item to determine bundle prefix: "
               << status;
    return std::nullopt;
  }

  CFStringRef access_group_ref =
      base::apple::GetValueFromDictionary<CFStringRef>(result.get(),
                                                       kSecAttrAccessGroup);

  if (!access_group_ref) {
    return std::nullopt;
  }

  // Format is "TeamID.BundleID" (e.g., "EQHXZ8M8AV.com.google.chrome.ios")
  std::string access_group = base::SysCFStringRefToUTF8(access_group_ref);
  auto components = base::SplitStringPiece(
      access_group, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return std::string(components[0]);
}

std::optional<std::string> GetAccessGroup() {
  auto appIdentifier = ComputeAppIdentifierPrefix();
  if (!appIdentifier.has_value()) {
    return std::nullopt;
  }
  return base::StrCat({*appIdentifier, ".", BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX),
                       ".devicetrust"});
}

}  // namespace

std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory() {
  PrivateKeyFactory::PrivateKeyFactoriesMap sub_factories;
  crypto::UnexportableKeyProvider::Config config;
  auto access_group = GetAccessGroup();
  if (access_group.has_value()) {
    config.keychain_access_group = access_group.value();
  }
  auto unexportable_key_factory =
      UnexportablePrivateKeyFactory::TryCreate(std::move(config));
  if (unexportable_key_factory) {
    sub_factories.insert_or_assign(PrivateKeySource::kUnexportableKey,
                                   std::move(unexportable_key_factory));
  } else {
    LOG(ERROR) << "Failed to create unexportable key factory.";
  }

  return PrivateKeyFactory::Create(std::move(sub_factories));
}

std::unique_ptr<client_certificates::CertificateProvisioningServiceIOS>
CreateBrowserCertificateProvisioningService(
    PrefService* local_state,
    client_certificates::CertificateStore* certificate_store,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!url_loader_factory || !device_management_service || !certificate_store) {
    return nullptr;
  }

  auto core_service =
      client_certificates::CertificateProvisioningService::Create(
          local_state, certificate_store,
          std::make_unique<BrowserContextDelegateIOS>(),
          client_certificates::KeyUploadClient::Create(
              std::make_unique<
                  enterprise_attestation::BrowserCloudManagementDelegate>(
                  enterprise_attestation::DMServerClient::Create(
                      device_management_service,
                      std::move(url_loader_factory)))));

  return client_certificates::CertificateProvisioningServiceIOS::Create(
      std::move(core_service));
}

}  // namespace client_certificates
