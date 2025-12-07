// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/feedback_private.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS)
class LogSourceAccessManager;
#endif  // BUILDFLAG(IS_CHROMEOS)

class FeedbackPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit FeedbackPrivateAPI(content::BrowserContext* context);

  FeedbackPrivateAPI(const FeedbackPrivateAPI&) = delete;
  FeedbackPrivateAPI& operator=(const FeedbackPrivateAPI&) = delete;

  ~FeedbackPrivateAPI() override;

  scoped_refptr<FeedbackService> GetService() const;

#if BUILDFLAG(IS_CHROMEOS)
  LogSourceAccessManager* GetLogSourceAccessManager() const;
#endif  // BUILDFLAG(IS_CHROMEOS)

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
      bool show_questionnaire,
      bool from_chrome_labs_or_kaleidoscope,
      bool from_autofill,
      const base::Value::Dict& autofill_metadata,
      const base::Value::Dict& ai_metadata);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>*
  GetFactoryInstance();

  // Use a custom FeedbackService implementation for tests.
  void SetFeedbackServiceForTesting(scoped_refptr<FeedbackService> service) {
    service_ = service;
  }

 private:
  friend class BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "FeedbackPrivateAPI"; }

  static const bool kServiceHasOwnInstanceInIncognito = true;

  const raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<FeedbackService> service_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<LogSourceAccessManager> log_source_access_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(IS_CHROMEOS)
 private:
  void OnCompleted(
      std::unique_ptr<api::feedback_private::ReadLogSourceResult> result);
#endif  // BUILDFLAG(IS_CHROMEOS)
};

class FeedbackPrivateSendFeedbackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.sendFeedback",
                             FEEDBACKPRIVATE_SENDFEEDBACK)

 protected:
  ~FeedbackPrivateSendFeedbackFunction() override {}
  ResponseAction Run() override;
  void OnCompleted(api::feedback_private::LandingPageType type, bool success);
};

class FeedbackPrivateOpenFeedbackFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.openFeedback",
                             FEEDBACKPRIVATE_OPENFEEDBACK)

 protected:
  ~FeedbackPrivateOpenFeedbackFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
