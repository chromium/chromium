// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/extension/built_in_web_extension.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <utility>
#import <vector>

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/callback.h"
#import "base/logging.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"

namespace web {

BuiltInWebExtension::BuiltInWebExtension() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

BuiltInWebExtension::~BuiltInWebExtension() = default;

WKWebExtension* BuiltInWebExtension::GetWKWebExtension() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return extension_;
}

void BuiltInWebExtension::CreateWKWebExtension(
    base::OnceCallback<void(WKWebExtension*)> completion_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (extension_ || loading_failed_) {
    std::move(completion_handler).Run(extension_);
    return;
  }

  // If `pending_callbacks_` is not empty, the extension is already being
  // loaded. Queue the callback and return early.
  bool extension_creation_in_progress = !pending_callbacks_.empty();

  pending_callbacks_.push_back(std::move(completion_handler));

  if (extension_creation_in_progress) {
    return;
  }

  __block auto loaded_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&BuiltInWebExtension::OnExtensionLoaded,
                                        weak_factory_.GetWeakPtr()));

  [WKWebExtension extensionWithResourceBaseURL:GetExtensionURL()
                             completionHandler:^(WKWebExtension* extension,
                                                 NSError* error) {
                               std::move(loaded_callback).Run(extension, error);
                             }];
}

void BuiltInWebExtension::OnExtensionLoaded(WKWebExtension* extension,
                                            NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error || !extension) {
    DLOG(ERROR) << "Failed to load built-in web extension: "
                << (error ? base::SysNSStringToUTF8(error.localizedDescription)
                          : "unknown error");
    base::debug::DumpWithoutCrashing();
  }
  extension_ = error ? nil : extension;
  loading_failed_ = !extension_;

  std::vector<base::OnceCallback<void(WKWebExtension*)>> callbacks =
      std::move(pending_callbacks_);
  pending_callbacks_.clear();

  for (auto& cb : callbacks) {
    std::move(cb).Run(extension_);
  }
}

}  // namespace web
