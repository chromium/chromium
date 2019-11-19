// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/feedback_private.h"
#include "ui/gfx/geometry/rect.h"

namespace feedback {
class FeedbackData;
}  // namespace feedback

namespace extensions {

class FeedbackService;
#if defined(OS_CHROMEOS)
class LogSourceAccessManager;
#endif  // defined(OS_CHROMEOS)

class FeedbackPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit FeedbackPrivateAPI(content::BrowserContext* context);
  ~FeedbackPrivateAPI() override;

  FeedbackService* GetService() const;

#if defined(OS_CHROMEOS)
  LogSourceAccessManager* GetLogSourceAccessManager() const;
#endif  // defined(OS_CHROMEOS)

  void RequestFeedbackForFlow(const std::string& description_template,
                              const std::string& description_placeholder_text,
                              const std::string& category_tag,
                              const std::string& extra_diagnostics,
                              const GURL& page_url,
                              api::feedback_private::FeedbackFlow flow,
                              bool from_assistant = false,
                              bool include_bluetooth_logs = false);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>*
  GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "FeedbackPrivateAPI"; }

  static const bool kServiceHasOwnInstanceInIncognito = true;

  content::BrowserContext* const browser_context_;
  std::unique_ptr<FeedbackService> service_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<LogSourceAccessManager> log_source_access_manager_;
#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(FeedbackPrivateAPI);
};

// Feedback strings.
class FeedbackPrivateGetStringsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getStrings",
                             FEEDBACKPRIVATE_GETSTRINGS)

  // Invoke this callback when this function is called - used for testing.
  static void set_test_callback(base::Closure* const callback) {
    test_callback_ = callback;
  }

 protected:
  ~FeedbackPrivateGetStringsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  static base::Closure* test_callback_;
};

class FeedbackPrivateGetUserEmailFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getUserEmail",
                             FEEDBACKPRIVATE_GETUSEREMAIL)

 protected:
  ~FeedbackPrivateGetUserEmailFunction() override {}
  ResponseAction Run() override;
};

class FeedbackPrivateGetSystemInformationFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getSystemInformation",
                             FEEDBACKPRIVATE_GETSYSTEMINFORMATION)

 protected:
  ~FeedbackPrivateGetSystemInformationFunction() override {}
  ResponseAction Run() override;

 private:
  void OnCompleted(std::unique_ptr<system_logs::SystemLogsResponse> sys_info);
};

// This function only reads from actual log sources on Chrome OS. On other
// platforms, it just returns EmptyResponse().
class FeedbackPrivateReadLogSourceFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.readLogSource",
                             FEEDBACKPRIVATE_READLOGSOURCE)

 protected:
  ~FeedbackPrivateReadLogSourceFunction() override {}
  ResponseAction Run() override;

#if defined(OS_CHROMEOS)
 private:
  void OnCompleted(
      std::unique_ptr<api::feedback_private::ReadLogSourceResult> result);
#endif  // defined(OS_CHROMEOS)
};

class FeedbackPrivateSendFeedbackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.sendFeedback",
                             FEEDBACKPRIVATE_SENDFEEDBACK)

 protected:
  ~FeedbackPrivateSendFeedbackFunction() override {}
  ResponseAction Run() override;

 private:
  void OnAllLogsFetched(bool send_histograms,
                        bool send_bluetooth_logs,
                        scoped_refptr<feedback::FeedbackData> feedback_data);
  void OnCompleted(api::feedback_private::LandingPageType type, bool success);
};

class FeedbackPrivateLoginFeedbackCompleteFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.loginFeedbackComplete",
                             FEEDBACKPRIVATE_LOGINFEEDBACKCOMPLETE)

 protected:
  ~FeedbackPrivateLoginFeedbackCompleteFunction() override {}
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
