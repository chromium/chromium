// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/feedback_private.h"
#include "ui/gfx/geometry/rect.h"

namespace feedback {
class FeedbackData;
}  // namespace feedback

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class LogSourceAccessManager;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class FeedbackPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit FeedbackPrivateAPI(content::BrowserContext* context);
  ~FeedbackPrivateAPI() override;

  FeedbackService* GetService() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  LogSourceAccessManager* GetLogSourceAccessManager() const;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Create a FeedbackInfo to be passed to UI/JS
  std::unique_ptr<api::feedback_private::FeedbackInfo> CreateFeedbackInfo(
      const std::string& description_template,
      const std::string& description_placeholder_text,
      const std::string& category_tag,
      const std::string& extra_diagnostics,
      const GURL& page_url,
      api::feedback_private::FeedbackFlow flow,
      bool from_assistant,
      bool include_bluetooth_logs,
      bool from_chrome_labs_or_kaleidoscope);

  void RequestFeedbackForFlow(const std::string& description_template,
                              const std::string& description_placeholder_text,
                              const std::string& category_tag,
                              const std::string& extra_diagnostics,
                              const GURL& page_url,
                              api::feedback_private::FeedbackFlow flow,
                              bool from_assistant = false,
                              bool include_bluetooth_logs = false,
                              bool from_chrome_labs_or_kaleidoscope = false);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>*
  GetFactoryInstance();

  // Use a custom FeedbackService implementation for tests.
  void SetFeedbackServiceForTesting(std::unique_ptr<FeedbackService> service) {
    service_ = std::move(service);
  }

 private:
  friend class BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "FeedbackPrivateAPI"; }

  static const bool kServiceHasOwnInstanceInIncognito = true;

  content::BrowserContext* const browser_context_;
  std::unique_ptr<FeedbackService> service_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<LogSourceAccessManager> log_source_access_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  DISALLOW_COPY_AND_ASSIGN(FeedbackPrivateAPI);
};

// Feedback strings.
class FeedbackPrivateGetStringsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getStrings",
                             FEEDBACKPRIVATE_GETSTRINGS)

  // Invoke this callback when this function is called - used for testing.
  static void set_test_callback(base::OnceClosure* callback) {
    test_callback_ = callback;
  }

 protected:
  ~FeedbackPrivateGetStringsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  static base::OnceClosure* test_callback_;
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

  bool send_all_crash_report_ids_;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  void OnCompleted(
      std::unique_ptr<api::feedback_private::ReadLogSourceResult> result);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
                        bool send_tab_titles,
                        scoped_refptr<feedback::FeedbackData> feedback_data);
  void OnCompleted(api::feedback_private::LandingPageType type, bool success);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnAshLogsFetched(bool send_histograms,
                        bool send_bluetooth_logs,
                        bool send_tab_titles,
                        scoped_refptr<feedback::FeedbackData> feedback_data);
  void OnLacrosHistogramsFetched(
      bool send_histograms,
      bool send_bluetooth_logs,
      bool send_tab_titles,
      scoped_refptr<feedback::FeedbackData> feedback_data,
      const std::string& compressed_histograms);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
