// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SETTINGS_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SETTINGS_CLIENT_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

namespace blink {

class WebURL;

// This class provides the content settings information which tells
// whether each feature is allowed or not.
class WebContentSettingsClient {
 public:
  // Only used if this is a WebContentSettingsClient on a worker thread. Clones
  // this WebContentSettingsClient so it can be used by another worker thread.
  virtual std::unique_ptr<WebContentSettingsClient> Clone() { return nullptr; }

  enum class StorageType {
    kDatabase,
    kCacheStorage,
    kIndexedDB,
    kFileSystem,
    kWebLocks,
    kLocalStorage,
    kSessionStorage
  };

  // Controls whether access to the given StorageType is allowed for this frame.
  // Runs asynchronously.
  virtual void AllowStorageAccess(StorageType storage_type,
                                  base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(true);
  }

  // Controls whether access to the given StorageType is allowed for this frame.
  // Blocks until done.
  virtual bool AllowStorageAccessSync(StorageType storage_type) { return true; }

  // Controls whether images are allowed for this frame.
  virtual bool AllowImage(bool enabled_per_settings, const WebURL& image_url) {
    return enabled_per_settings;
  }

  // Controls whether scripts are allowed to execute for this frame.
  virtual bool AllowScript(bool enabled_per_settings) {
    return enabled_per_settings;
  }

  // Controls whether scripts loaded from the given URL are allowed to execute
  // for this frame.
  virtual bool AllowScriptFromSource(bool enabled_per_settings,
                                     const WebURL& script_url) {
    return enabled_per_settings;
  }

  // Controls whether insecure scripts are allowed to execute for this frame.
  virtual bool AllowRunningInsecureContent(bool enabled_per_settings,
                                           const WebURL&) {
    return enabled_per_settings;
  }

  // Controls whether auto dark web content is allowed for this frame.
  virtual bool AllowAutoDarkWebContent(bool enabled_per_settings) {
    return enabled_per_settings;
  }

  // Controls whether access to read the clipboard is allowed for this frame.
  virtual bool AllowReadFromClipboard(bool default_value) {
    return default_value;
  }

  // Controls whether access to write the clipboard is allowed for this frame.
  virtual bool AllowWriteToClipboard(bool default_value) {
    return default_value;
  }

  // Controls whether enabling Web Components API for this frame.
  virtual bool AllowWebComponents(bool default_value) { return default_value; }

  // Controls whether to enable MutationEvents for this frame.
  // The common use case of this method is actually to selectively disable
  // MutationEvents, but it's been named for consistency with the rest of the
  // interface.
  virtual bool AllowMutationEvents(bool default_value) { return default_value; }

  virtual bool AllowPopupsAndRedirects(bool default_value) {
    return default_value;
  }

  // Reports that passive mixed content was found at the provided URL.
  virtual void PassiveInsecureContentFound(const WebURL&) {}

  // Notifies the client that the frame would have executed script if script
  // were enabled.
  virtual void DidNotAllowScript() {}

  // Called to persist the received client hint preferences when |url| was
  // fetched. The preferences should be persisted for |duration|.
  virtual void PersistClientHints(
      const EnabledClientHints& enabled_client_hints,
      base::TimeDelta duration,
      const blink::WebURL& url) {}

  // Controls whether mixed content autoupgrades should be allowed in this
  // frame.
  virtual bool ShouldAutoupgradeMixedContent() { return true; }

  // Controls whether the ViewTransition callback needs to be larger than
  // default.
  virtual bool IncreaseViewTransitionCallbackTimeout() const { return false; }

  virtual ~WebContentSettingsClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SETTINGS_CLIENT_H_
