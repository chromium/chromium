// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_

#import <memory>
#import <string>

#import "base/scoped_observation.h"
#import "components/autofill/core/browser/logging/log_manager.h"
#import "components/password_manager/core/browser/password_reuse_manager.h"
#import "components/password_manager/ios/password_reuse_detection_manager_client_bridge.h"
#import "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager.h"
#import "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager_client.h"
#import "ios/chrome/browser/safe_browsing/model/input_event_observer.h"
#import "ios/chrome/browser/safe_browsing/model/password_protection_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace safe_browsing {
class PasswordProtectionService;
enum class WarningAction;
}  // namespace safe_browsing

namespace autofill {
class LogManager;
}

@protocol IOSChromePasswordReuseDetectionManagerClientBridge <
    PasswordReuseDetectionManagerClientBridge>

@property(readonly, nonatomic) ProfileIOS* profile;

@end

// An iOS implementation of
// safe_browsing::PasswordReuseDetectionManagerClient.
class IOSChromePasswordReuseDetectionManagerClient
    : public safe_browsing::PasswordReuseDetectionManagerClient,
      public web::WebStateObserver,
      public InputEventObserver {
 public:
  explicit IOSChromePasswordReuseDetectionManagerClient(
      id<IOSChromePasswordReuseDetectionManagerClientBridge> bridge);

  IOSChromePasswordReuseDetectionManagerClient(
      const IOSChromePasswordReuseDetectionManagerClient&) = delete;
  IOSChromePasswordReuseDetectionManagerClient& operator=(
      const IOSChromePasswordReuseDetectionManagerClient&) = delete;

  ~IOSChromePasswordReuseDetectionManagerClient() override;

  // Returns the committed main frame URL.
  const GURL& GetLastCommittedURL() const;

  // safe_browsing::PasswordReuseDetectionManagerClient implementation.
  password_manager::PasswordReuseManager* GetPasswordReuseManager()
      const override;
  autofill::LogManager* GetLogManager() override;
  virtual safe_browsing::PasswordProtectionService*
  GetPasswordProtectionService() const;

  bool IsHistorySyncAccountEmail(const std::string& username) override;

  bool IsPasswordFieldDetectedOnPage() override;

  void CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists,
      uint64_t reused_password_hash,
      const std::string& domain) override;

  void MaybeLogPasswordReuseDetectedEvent() override;

  // Shows the password protection UI. `warning_text` is the displayed text.
  // `callback` is invoked when the user dismisses the UI.
  void NotifyUserPasswordProtectionWarning(
      const std::u16string& warning_text,
      base::OnceCallback<void(safe_browsing::WarningAction)> callback);

 private:
  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // InputEventObserver:
  void OnKeyPressed(std::string text) override;
  void OnPaste(std::string text) override;
  web::WebState* web_state() const override;

  __weak id<IOSChromePasswordReuseDetectionManagerClientBridge> bridge_;

  safe_browsing::PasswordReuseDetectionManager
      password_reuse_detection_manager_;

  std::unique_ptr<autofill::LogManager> log_manager_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  base::ScopedObservation<PasswordProtectionJavaScriptFeature,
                          InputEventObserver>
      input_event_observation_{this};
  base::WeakPtrFactory<IOSChromePasswordReuseDetectionManagerClient>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_H_
