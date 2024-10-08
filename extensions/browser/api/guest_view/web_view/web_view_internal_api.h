// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_GUEST_VIEW_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_GUEST_VIEW_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/api/web_contents_capture_client.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/guest_view/web_view/web_ui/web_ui_url_fetcher.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/url_fetcher.h"

namespace base {
class TaskRunner;
}

// WARNING: WebViewInternal could be loaded in an unprivileged context, thus any
// new APIs must extend WebViewInternalExtensionFunction or
// WebViewInternalExecuteCodeFunction which do a process ID check to prevent
// abuse by normal renderer processes.
namespace extensions {

class WebViewInternalExtensionFunction : public ExtensionFunction {
 public:
  WebViewInternalExtensionFunction() = default;

 protected:
  ~WebViewInternalExtensionFunction() override = default;
  bool PreRunValidation(std::string* error) override;

  WebViewGuest& GetGuest();

 private:
  int instance_id_ = 0;
};

class WebViewInternalCaptureVisibleRegionFunction
    : public WebViewInternalExtensionFunction,
      public WebContentsCaptureClient {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.captureVisibleRegion",
                             WEBVIEWINTERNAL_CAPTUREVISIBLEREGION)
  WebViewInternalCaptureVisibleRegionFunction();

  WebViewInternalCaptureVisibleRegionFunction(
      const WebViewInternalCaptureVisibleRegionFunction&) = delete;
  WebViewInternalCaptureVisibleRegionFunction& operator=(
      const WebViewInternalCaptureVisibleRegionFunction&) = delete;

 protected:
  ~WebViewInternalCaptureVisibleRegionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  bool ShouldSkipQuotaLimiting() const override;

 private:
  // extensions::WebContentsCaptureClient:
  ScreenshotAccess GetScreenshotAccess(
      content::WebContents* web_contents) const override;
  bool ClientAllowsTransparency() override;
  void OnCaptureSuccess(const SkBitmap& bitmap) override;
  void OnCaptureFailure(CaptureResult result) override;

  void EncodeBitmapOnWorkerThread(
      scoped_refptr<base::TaskRunner> reply_task_runner,
      const SkBitmap& bitmap);
  void OnBitmapEncodedOnUIThread(bool success, std::string base64_result);

  std::string GetErrorMessage(CaptureResult result);

  bool is_guest_transparent_;
};

class WebViewInternalNavigateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.navigate",
                             WEBVIEWINTERNAL_NAVIGATE)
  WebViewInternalNavigateFunction() {}

  WebViewInternalNavigateFunction(const WebViewInternalNavigateFunction&) =
      delete;
  WebViewInternalNavigateFunction& operator=(
      const WebViewInternalNavigateFunction&) = delete;

 protected:
  ~WebViewInternalNavigateFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalExecuteCodeFunction
    : public extensions::ExecuteCodeFunction {
 public:
  // This is called when a file URL request is complete.
  // Parameters:
  // - whether the request is success.
  // - If yes, the content of the file.
  // This callback should match the associated LoadFileCallback types
  // specified in WebUIURLFetcher and ControlledFrameEmbedderURLFetcher.
  using LoadFileCallback =
      base::OnceCallback<void(bool, std::unique_ptr<std::string>)>;

  WebViewInternalExecuteCodeFunction();

  WebViewInternalExecuteCodeFunction(
      const WebViewInternalExecuteCodeFunction&) = delete;
  WebViewInternalExecuteCodeFunction& operator=(
      const WebViewInternalExecuteCodeFunction&) = delete;

 protected:
  ~WebViewInternalExecuteCodeFunction() override;

  // Initialize |details_| if it hasn't already been.
  InitResult Init() override;
  bool ShouldInsertCSS() const override;
  bool ShouldRemoveCSS() const override;
  bool CanExecuteScriptOnPage(std::string* error) override;
  // Guarded by a process ID check.
  extensions::ScriptExecutor* GetScriptExecutor(std::string* error) final;
  bool IsWebView() const override;
  const GURL& GetWebViewSrc() const override;
  bool LoadFile(const std::string& file, std::string* error) override;

 private:
  // Loads a file url in embedders such as WebUI and Controlled Frame.
  bool LoadFileForEmbedder(const std::string& file_src,
                           LoadFileCallback callback);
  void DidLoadFileForEmbedder(const std::string& file,
                              bool success,
                              std::unique_ptr<std::string> data);

  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  extensions::ExtensionResource resource_;

  int guest_instance_id_;

  GURL guest_src_;

  std::unique_ptr<URLFetcher> url_fetcher_;
};

class WebViewInternalExecuteScriptFunction
    : public WebViewInternalExecuteCodeFunction {
 public:
  WebViewInternalExecuteScriptFunction();

  WebViewInternalExecuteScriptFunction(
      const WebViewInternalExecuteScriptFunction&) = delete;
  WebViewInternalExecuteScriptFunction& operator=(
      const WebViewInternalExecuteScriptFunction&) = delete;

 protected:
  ~WebViewInternalExecuteScriptFunction() override {}

  DECLARE_EXTENSION_FUNCTION("webViewInternal.executeScript",
                             WEBVIEWINTERNAL_EXECUTESCRIPT)
};

class WebViewInternalInsertCSSFunction
    : public WebViewInternalExecuteCodeFunction {
 public:
  WebViewInternalInsertCSSFunction();

  WebViewInternalInsertCSSFunction(const WebViewInternalInsertCSSFunction&) =
      delete;
  WebViewInternalInsertCSSFunction& operator=(
      const WebViewInternalInsertCSSFunction&) = delete;

 protected:
  ~WebViewInternalInsertCSSFunction() override {}

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("webViewInternal.insertCSS",
                             WEBVIEWINTERNAL_INSERTCSS)
};

class WebViewInternalAddContentScriptsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.addContentScripts",
                             WEBVIEWINTERNAL_ADDCONTENTSCRIPTS)

  WebViewInternalAddContentScriptsFunction();

  WebViewInternalAddContentScriptsFunction(
      const WebViewInternalAddContentScriptsFunction&) = delete;
  WebViewInternalAddContentScriptsFunction& operator=(
      const WebViewInternalAddContentScriptsFunction&) = delete;

 protected:
  ~WebViewInternalAddContentScriptsFunction() override;

 private:
  ExecuteCodeFunction::ResponseAction Run() override;
};

class WebViewInternalRemoveContentScriptsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.removeContentScripts",
                             WEBVIEWINTERNAL_REMOVECONTENTSCRIPTS)

  WebViewInternalRemoveContentScriptsFunction();

  WebViewInternalRemoveContentScriptsFunction(
      const WebViewInternalRemoveContentScriptsFunction&) = delete;
  WebViewInternalRemoveContentScriptsFunction& operator=(
      const WebViewInternalRemoveContentScriptsFunction&) = delete;

 protected:
  ~WebViewInternalRemoveContentScriptsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalSetNameFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setName", WEBVIEWINTERNAL_SETNAME)

  WebViewInternalSetNameFunction();

  WebViewInternalSetNameFunction(const WebViewInternalSetNameFunction&) =
      delete;
  WebViewInternalSetNameFunction& operator=(
      const WebViewInternalSetNameFunction&) = delete;

 protected:
  ~WebViewInternalSetNameFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalSetAllowTransparencyFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAllowTransparency",
                             WEBVIEWINTERNAL_SETALLOWTRANSPARENCY)

  WebViewInternalSetAllowTransparencyFunction();

  WebViewInternalSetAllowTransparencyFunction(
      const WebViewInternalSetAllowTransparencyFunction&) = delete;
  WebViewInternalSetAllowTransparencyFunction& operator=(
      const WebViewInternalSetAllowTransparencyFunction&) = delete;

 protected:
  ~WebViewInternalSetAllowTransparencyFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalSetAllowScalingFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAllowScaling",
                             WEBVIEWINTERNAL_SETALLOWSCALING)

  WebViewInternalSetAllowScalingFunction();

  WebViewInternalSetAllowScalingFunction(
      const WebViewInternalSetAllowScalingFunction&) = delete;
  WebViewInternalSetAllowScalingFunction& operator=(
      const WebViewInternalSetAllowScalingFunction&) = delete;

 protected:
  ~WebViewInternalSetAllowScalingFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalSetZoomFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setZoom", WEBVIEWINTERNAL_SETZOOM)

  WebViewInternalSetZoomFunction();

  WebViewInternalSetZoomFunction(const WebViewInternalSetZoomFunction&) =
      delete;
  WebViewInternalSetZoomFunction& operator=(
      const WebViewInternalSetZoomFunction&) = delete;

 protected:
  ~WebViewInternalSetZoomFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalGetZoomFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getZoom", WEBVIEWINTERNAL_GETZOOM)

  WebViewInternalGetZoomFunction();

  WebViewInternalGetZoomFunction(const WebViewInternalGetZoomFunction&) =
      delete;
  WebViewInternalGetZoomFunction& operator=(
      const WebViewInternalGetZoomFunction&) = delete;

 protected:
  ~WebViewInternalGetZoomFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalSetZoomModeFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setZoomMode",
                             WEBVIEWINTERNAL_SETZOOMMODE)

  WebViewInternalSetZoomModeFunction();

  WebViewInternalSetZoomModeFunction(
      const WebViewInternalSetZoomModeFunction&) = delete;
  WebViewInternalSetZoomModeFunction& operator=(
      const WebViewInternalSetZoomModeFunction&) = delete;

 protected:
  ~WebViewInternalSetZoomModeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalGetZoomModeFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getZoomMode",
                             WEBVIEWINTERNAL_GETZOOMMODE)

  WebViewInternalGetZoomModeFunction();

  WebViewInternalGetZoomModeFunction(
      const WebViewInternalGetZoomModeFunction&) = delete;
  WebViewInternalGetZoomModeFunction& operator=(
      const WebViewInternalGetZoomModeFunction&) = delete;

 protected:
  ~WebViewInternalGetZoomModeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalFindFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.find", WEBVIEWINTERNAL_FIND)

  WebViewInternalFindFunction();

  WebViewInternalFindFunction(const WebViewInternalFindFunction&) = delete;
  WebViewInternalFindFunction& operator=(const WebViewInternalFindFunction&) =
      delete;

  // Used by WebViewInternalFindHelper to Respond().
  void ForwardResponse(base::Value::Dict results);

 protected:
  ~WebViewInternalFindFunction() override;

 private:
  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalStopFindingFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.stopFinding",
                             WEBVIEWINTERNAL_STOPFINDING)

  WebViewInternalStopFindingFunction();

  WebViewInternalStopFindingFunction(
      const WebViewInternalStopFindingFunction&) = delete;
  WebViewInternalStopFindingFunction& operator=(
      const WebViewInternalStopFindingFunction&) = delete;

 protected:
  ~WebViewInternalStopFindingFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalLoadDataWithBaseUrlFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.loadDataWithBaseUrl",
                             WEBVIEWINTERNAL_LOADDATAWITHBASEURL)

  WebViewInternalLoadDataWithBaseUrlFunction();

  WebViewInternalLoadDataWithBaseUrlFunction(
      const WebViewInternalLoadDataWithBaseUrlFunction&) = delete;
  WebViewInternalLoadDataWithBaseUrlFunction& operator=(
      const WebViewInternalLoadDataWithBaseUrlFunction&) = delete;

 protected:
  ~WebViewInternalLoadDataWithBaseUrlFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalGoFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.go", WEBVIEWINTERNAL_GO)

  WebViewInternalGoFunction();

  WebViewInternalGoFunction(const WebViewInternalGoFunction&) = delete;
  WebViewInternalGoFunction& operator=(const WebViewInternalGoFunction&) =
      delete;

 protected:
  ~WebViewInternalGoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalReloadFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.reload", WEBVIEWINTERNAL_RELOAD)

  WebViewInternalReloadFunction();

  WebViewInternalReloadFunction(const WebViewInternalReloadFunction&) = delete;
  WebViewInternalReloadFunction& operator=(
      const WebViewInternalReloadFunction&) = delete;

 protected:
  ~WebViewInternalReloadFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalSetPermissionFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setPermission",
                             WEBVIEWINTERNAL_SETPERMISSION)

  WebViewInternalSetPermissionFunction();

  WebViewInternalSetPermissionFunction(
      const WebViewInternalSetPermissionFunction&) = delete;
  WebViewInternalSetPermissionFunction& operator=(
      const WebViewInternalSetPermissionFunction&) = delete;

 protected:
  ~WebViewInternalSetPermissionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalOverrideUserAgentFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.overrideUserAgent",
                             WEBVIEWINTERNAL_OVERRIDEUSERAGENT)

  WebViewInternalOverrideUserAgentFunction();

  WebViewInternalOverrideUserAgentFunction(
      const WebViewInternalOverrideUserAgentFunction&) = delete;
  WebViewInternalOverrideUserAgentFunction& operator=(
      const WebViewInternalOverrideUserAgentFunction&) = delete;

 protected:
  ~WebViewInternalOverrideUserAgentFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalStopFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.stop", WEBVIEWINTERNAL_STOP)

  WebViewInternalStopFunction();

  WebViewInternalStopFunction(const WebViewInternalStopFunction&) = delete;
  WebViewInternalStopFunction& operator=(const WebViewInternalStopFunction&) =
      delete;

 protected:
  ~WebViewInternalStopFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalSetAudioMutedFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAudioMuted",
                             WEBVIEWINTERNAL_SETAUDIOMUTED)

  WebViewInternalSetAudioMutedFunction();

  WebViewInternalSetAudioMutedFunction(
      const WebViewInternalSetAudioMutedFunction&) = delete;
  WebViewInternalSetAudioMutedFunction& operator=(
      const WebViewInternalSetAudioMutedFunction&) = delete;

 protected:
  ~WebViewInternalSetAudioMutedFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalIsAudioMutedFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.isAudioMuted",
                             WEBVIEWINTERNAL_ISAUDIOMUTED)

  WebViewInternalIsAudioMutedFunction();

  WebViewInternalIsAudioMutedFunction(
      const WebViewInternalIsAudioMutedFunction&) = delete;
  WebViewInternalIsAudioMutedFunction& operator=(
      const WebViewInternalIsAudioMutedFunction&) = delete;

 protected:
  ~WebViewInternalIsAudioMutedFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalGetAudioStateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getAudioState",
                             WEBVIEWINTERNAL_GETAUDIOSTATE)

  WebViewInternalGetAudioStateFunction();

  WebViewInternalGetAudioStateFunction(
      const WebViewInternalGetAudioStateFunction&) = delete;
  WebViewInternalGetAudioStateFunction& operator=(
      const WebViewInternalGetAudioStateFunction&) = delete;

 protected:
  ~WebViewInternalGetAudioStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalTerminateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.terminate",
                             WEBVIEWINTERNAL_TERMINATE)

  WebViewInternalTerminateFunction();

  WebViewInternalTerminateFunction(const WebViewInternalTerminateFunction&) =
      delete;
  WebViewInternalTerminateFunction& operator=(
      const WebViewInternalTerminateFunction&) = delete;

 protected:
  ~WebViewInternalTerminateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalClearDataFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.clearData",
                             WEBVIEWINTERNAL_CLEARDATA)

  WebViewInternalClearDataFunction();

  WebViewInternalClearDataFunction(const WebViewInternalClearDataFunction&) =
      delete;
  WebViewInternalClearDataFunction& operator=(
      const WebViewInternalClearDataFunction&) = delete;

 protected:
  ~WebViewInternalClearDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  uint32_t GetRemovalMask();
  void ClearDataDone();

  // Removal start time.
  base::Time remove_since_;
  // Removal mask, corresponds to StoragePartition::RemoveDataMask enum.
  uint32_t remove_mask_;
  // Tracks any data related or parse errors.
  bool bad_message_;
};

class WebViewInternalSetSpatialNavigationEnabledFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setSpatialNavigationEnabled",
                             WEBVIEWINTERNAL_SETSPATIALNAVIGATIONENABLED)

  WebViewInternalSetSpatialNavigationEnabledFunction();

  WebViewInternalSetSpatialNavigationEnabledFunction(
      const WebViewInternalSetSpatialNavigationEnabledFunction&) = delete;
  WebViewInternalSetSpatialNavigationEnabledFunction& operator=(
      const WebViewInternalSetSpatialNavigationEnabledFunction&) = delete;

 protected:
  ~WebViewInternalSetSpatialNavigationEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebViewInternalIsSpatialNavigationEnabledFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.isSpatialNavigationEnabled",
                             WEBVIEWINTERNAL_ISSPATIALNAVIGATIONENABLED)

  WebViewInternalIsSpatialNavigationEnabledFunction();

  WebViewInternalIsSpatialNavigationEnabledFunction(
      const WebViewInternalIsSpatialNavigationEnabledFunction&) = delete;
  WebViewInternalIsSpatialNavigationEnabledFunction& operator=(
      const WebViewInternalIsSpatialNavigationEnabledFunction&) = delete;

 protected:
  ~WebViewInternalIsSpatialNavigationEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_GUEST_VIEW_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_
