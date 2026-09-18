// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/extension/extension_controller_impl.h"

#import <WebKit/WebKit.h>

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/extension/built_in_web_extension.h"
#import "ios/web/extension/gpc_web_extension/gpc_web_extension.h"

namespace {

// Returns the BuiltInWebExtension instance for the given `extension`.
API_AVAILABLE(ios(18.4))
web::BuiltInWebExtension* GetBuiltInWebExtension(
    web::BuiltInExtension extension) {
  switch (extension) {
    case web::BuiltInExtension::kGPC:
      return web::GPCWebExtension::GetInstance();
  }
}

}  // namespace

namespace web {

#pragma mark - ExtensionController static

// static
std::unique_ptr<ExtensionController> ExtensionController::Create() {
  if (@available(iOS 18.4, *)) {
    return std::make_unique<ExtensionControllerImpl>();
  }
  return nullptr;
}

#pragma mark - Lifecycle

ExtensionControllerImpl::ExtensionControllerImpl() {
  WKWebExtensionControllerConfiguration* config =
      [WKWebExtensionControllerConfiguration defaultConfiguration];
  extension_controller_ =
      [[WKWebExtensionController alloc] initWithConfiguration:config];
}

ExtensionControllerImpl::~ExtensionControllerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

#pragma mark - ExtensionController

void ExtensionControllerImpl::LoadBuiltInExtension(
    BuiltInExtension extension,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsBuiltInExtensionLoaded(extension)) {
    std::move(callback).Run(true);
    return;
  }

  GetBuiltInWebExtension(extension)->CreateWKWebExtension(base::BindOnce(
      &ExtensionControllerImpl::OnBuiltInWebExtensionCreated,
      weak_ptr_factory_.GetWeakPtr(), extension, std::move(callback)));
}

void ExtensionControllerImpl::OnBuiltInWebExtensionCreated(
    BuiltInExtension extension,
    base::OnceCallback<void(bool)> callback,
    WKWebExtension* wk_extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!wk_extension) {
    std::move(callback).Run(false);
    return;
  }

  if (IsBuiltInExtensionLoaded(extension)) {
    std::move(callback).Run(true);
    return;
  }

  WKWebExtensionContext* context =
      [WKWebExtensionContext contextForExtension:wk_extension];

  // Built-in extensions are bundled and trusted. Grant all requested
  // permissions upfront so the extension operates without user prompting.
  for (WKWebExtensionPermission permission in wk_extension
           .requestedPermissions) {
    [context setPermissionStatus:
                 WKWebExtensionContextPermissionStatusGrantedExplicitly
                   forPermission:permission];
  }

  // Grant all requested match patterns upfront so the extension has access to
  // all specified URLs.
  for (WKWebExtensionMatchPattern* pattern in wk_extension
           .requestedPermissionMatchPatterns) {
    [context setPermissionStatus:
                 WKWebExtensionContextPermissionStatusGrantedExplicitly
                 forMatchPattern:pattern];
  }

  NSError* context_error = nil;
  BOOL success = [extension_controller_ loadExtensionContext:context
                                                       error:&context_error];
  if (!success || context_error) {
    DLOG(ERROR) << "Failed to load extension context: "
                << base::SysNSStringToUTF8(context_error.localizedDescription);
  }
  std::move(callback).Run(success);
}

void ExtensionControllerImpl::UnloadBuiltInExtension(
    BuiltInExtension extension,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WKWebExtension* wk_extension =
      GetBuiltInWebExtension(extension)->GetWKWebExtension();
  if (!wk_extension) {
    std::move(callback).Run(false);
    return;
  }

  WKWebExtensionContext* context =
      [extension_controller_ extensionContextForExtension:wk_extension];
  if (!context) {
    std::move(callback).Run(false);
    return;
  }

  NSError* error = nil;
  BOOL unloaded = [extension_controller_ unloadExtensionContext:context
                                                          error:&error];
  if (!unloaded || error) {
    DLOG(ERROR) << "Failed to unload extension context: "
                << base::SysNSStringToUTF8(error.localizedDescription);
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

bool ExtensionControllerImpl::IsBuiltInExtensionLoaded(
    BuiltInExtension extension) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WKWebExtension* wk_extension =
      GetBuiltInWebExtension(extension)->GetWKWebExtension();
  if (!wk_extension) {
    return false;
  }
  return
      [extension_controller_ extensionContextForExtension:wk_extension] != nil;
}

WKWebExtensionController* ExtensionControllerImpl::GetWKWebExtensionController()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return extension_controller_;
}

}  // namespace web
