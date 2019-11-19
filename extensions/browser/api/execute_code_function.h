// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_H_
#define EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_H_

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/host_id.h"

namespace extensions {

// Base class for javascript code injection.
// This is used by both chrome.webview.executeScript and
// chrome.tabs.executeScript.
class ExecuteCodeFunction : public ExtensionFunction {
 public:
  ExecuteCodeFunction();

 protected:
  ~ExecuteCodeFunction() override;

  // ExtensionFunction implementation.
  ResponseAction Run() override;

  enum InitResult {
    // ExtensionFunction validation failure.
    // Warning: Validation failures kill the renderer and should only be used
    // for circumstances that should never happen.
    VALIDATION_FAILURE,
    // Failure other than validation.
    // Failures return an error to the extension function and should be used
    // when an error situation occurs: e.g. trying to execute script during
    // browser shutdown.
    FAILURE,
    SUCCESS
  };

  // Initializes |details_| and other variables if they haven't already been.
  // Returns whether or not it succeeded. Failure can be tolerable (FAILURE), or
  // fatal (VALIDATION_FAILURE).
  virtual InitResult Init() = 0;
  virtual bool ShouldInsertCSS() const = 0;
  virtual bool CanExecuteScriptOnPage(std::string* error) = 0;
  virtual ScriptExecutor* GetScriptExecutor(std::string* error) = 0;
  virtual bool IsWebView() const = 0;
  virtual const GURL& GetWebViewSrc() const = 0;
  virtual bool LoadFile(const std::string& file, std::string* error);

  // Called when contents from the loaded file have been localized.
  void DidLoadAndLocalizeFile(const std::string& file,
                              bool success,
                              std::unique_ptr<std::string> data);

  const HostID& host_id() const { return host_id_; }
  void set_host_id(const HostID& host_id) { host_id_ = host_id; }

  InitResult set_init_result(InitResult init_result) {
    init_result_ = init_result;
    return init_result_.value();
  }
  InitResult set_init_result_error(const std::string& error) {
    init_error_ = error;
    return set_init_result(FAILURE);
  }

  // The injection details.
  std::unique_ptr<api::extension_types::InjectDetails> details_;
  base::Optional<InitResult> init_result_;
  // Set iff |init_result_| == FAILURE, holds the error string.
  base::Optional<std::string> init_error_;

 private:
  void OnExecuteCodeFinished(const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& result);

  // Retrieves the file url for the given |extension_path| and optionally
  // localizes |data|.
  // Localization depends on whether |might_require_localization| was specified.
  // Only CSS file content needs to be localized.
  void GetFileURLAndMaybeLocalizeInBackground(
      const std::string& extension_id,
      const base::FilePath& extension_path,
      const std::string& extension_default_locale,
      bool might_require_localization,
      std::string* data);

  // Retrieves the file url for the given |extension_path| and optionally
  // localizes |data|.
  // Similar to GetFileURLAndMaybeLocalizeInBackground, but only applies
  // to component extension resource.
  std::unique_ptr<std::string>
  GetFileURLAndLocalizeComponentResourceInBackground(
      std::unique_ptr<std::string> data,
      const std::string& extension_id,
      const base::FilePath& extension_path,
      const std::string& extension_default_locale,
      bool might_require_localization);

  // Run in UI thread.  Code string contains the code to be executed. Returns
  // true on success. If true is returned, this does an AddRef. Returns false on
  // failure and sets |error|.
  bool Execute(const std::string& code_string, std::string* error);

  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  ExtensionResource resource_;

  // The URL of the file being injected into the page.
  GURL file_url_;

  // The ID of the injection host.
  HostID host_id_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteCodeFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_H_
