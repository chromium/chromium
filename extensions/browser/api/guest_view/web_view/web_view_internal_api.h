// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_GUEST_VIEW_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_GUEST_VIEW_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_

#include <stdint.h>

#include "base/macros.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/api/web_contents_capture_client.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/guest_view/web_view/web_ui/web_ui_url_fetcher.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

// WARNING: WebViewInternal could be loaded in an unblessed context, thus any
// new APIs must extend WebViewInternalExtensionFunction or
// WebViewInternalExecuteCodeFunction which do a process ID check to prevent
// abuse by normal renderer processes.
namespace extensions {

class WebViewInternalExtensionFunction : public ExtensionFunction {
 public:
  WebViewInternalExtensionFunction() {}

 protected:
  ~WebViewInternalExtensionFunction() override {}
  bool PreRunValidation(std::string* error) override;

  WebViewGuest* guest_ = nullptr;
};

class WebViewInternalCaptureVisibleRegionFunction
    : public WebViewInternalExtensionFunction,
      public WebContentsCaptureClient {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.captureVisibleRegion",
                             WEBVIEWINTERNAL_CAPTUREVISIBLEREGION)
  WebViewInternalCaptureVisibleRegionFunction();

 protected:
  ~WebViewInternalCaptureVisibleRegionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // extensions::WebContentsCaptureClient:
  bool IsScreenshotEnabled() const override;
  bool ClientAllowsTransparency() override;
  void OnCaptureSuccess(const SkBitmap& bitmap) override;
  void OnCaptureFailure(CaptureResult result) override;

  std::string GetErrorMessage(CaptureResult result);

  bool is_guest_transparent_;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalCaptureVisibleRegionFunction);
};

class WebViewInternalNavigateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.navigate",
                             WEBVIEWINTERNAL_NAVIGATE)
  WebViewInternalNavigateFunction() {}

 protected:
  ~WebViewInternalNavigateFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalNavigateFunction);
};

class WebViewInternalExecuteCodeFunction
    : public extensions::ExecuteCodeFunction {
 public:
  WebViewInternalExecuteCodeFunction();

 protected:
  ~WebViewInternalExecuteCodeFunction() override;

  // Initialize |details_| if it hasn't already been.
  InitResult Init() override;
  bool ShouldInsertCSS() const override;
  bool CanExecuteScriptOnPage(std::string* error) override;
  // Guarded by a process ID check.
  extensions::ScriptExecutor* GetScriptExecutor(std::string* error) final;
  bool IsWebView() const override;
  const GURL& GetWebViewSrc() const override;
  bool LoadFile(const std::string& file, std::string* error) override;

 private:
  // Loads a file url on WebUI.
  bool LoadFileForWebUI(const std::string& file_src,
                        WebUIURLFetcher::WebUILoadFileCallback callback);

  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  extensions::ExtensionResource resource_;

  int guest_instance_id_;

  GURL guest_src_;

  std::unique_ptr<WebUIURLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalExecuteCodeFunction);
};

class WebViewInternalExecuteScriptFunction
    : public WebViewInternalExecuteCodeFunction {
 public:
  WebViewInternalExecuteScriptFunction();

 protected:
  ~WebViewInternalExecuteScriptFunction() override {}

  DECLARE_EXTENSION_FUNCTION("webViewInternal.executeScript",
                             WEBVIEWINTERNAL_EXECUTESCRIPT)

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalExecuteScriptFunction);
};

class WebViewInternalInsertCSSFunction
    : public WebViewInternalExecuteCodeFunction {
 public:
  WebViewInternalInsertCSSFunction();

 protected:
  ~WebViewInternalInsertCSSFunction() override {}

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("webViewInternal.insertCSS",
                             WEBVIEWINTERNAL_INSERTCSS)

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalInsertCSSFunction);
};

class WebViewInternalAddContentScriptsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.addContentScripts",
                             WEBVIEWINTERNAL_ADDCONTENTSCRIPTS)

  WebViewInternalAddContentScriptsFunction();

 protected:
  ~WebViewInternalAddContentScriptsFunction() override;

 private:
  ExecuteCodeFunction::ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalAddContentScriptsFunction);
};

class WebViewInternalRemoveContentScriptsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.removeContentScripts",
                             WEBVIEWINTERNAL_REMOVECONTENTSCRIPTS)

  WebViewInternalRemoveContentScriptsFunction();

 protected:
  ~WebViewInternalRemoveContentScriptsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalRemoveContentScriptsFunction);
};

class WebViewInternalSetNameFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setName", WEBVIEWINTERNAL_SETNAME)

  WebViewInternalSetNameFunction();

 protected:
  ~WebViewInternalSetNameFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetNameFunction);
};

class WebViewInternalSetAllowTransparencyFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAllowTransparency",
                             WEBVIEWINTERNAL_SETALLOWTRANSPARENCY)

  WebViewInternalSetAllowTransparencyFunction();

 protected:
  ~WebViewInternalSetAllowTransparencyFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetAllowTransparencyFunction);
};

class WebViewInternalSetAllowScalingFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAllowScaling",
                             WEBVIEWINTERNAL_SETALLOWSCALING)

  WebViewInternalSetAllowScalingFunction();

 protected:
  ~WebViewInternalSetAllowScalingFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetAllowScalingFunction);
};

class WebViewInternalSetZoomFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setZoom", WEBVIEWINTERNAL_SETZOOM)

  WebViewInternalSetZoomFunction();

 protected:
  ~WebViewInternalSetZoomFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetZoomFunction);
};

class WebViewInternalGetZoomFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getZoom", WEBVIEWINTERNAL_GETZOOM)

  WebViewInternalGetZoomFunction();

 protected:
  ~WebViewInternalGetZoomFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetZoomFunction);
};

class WebViewInternalSetZoomModeFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setZoomMode",
                             WEBVIEWINTERNAL_SETZOOMMODE)

  WebViewInternalSetZoomModeFunction();

 protected:
  ~WebViewInternalSetZoomModeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetZoomModeFunction);
};

class WebViewInternalGetZoomModeFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getZoomMode",
                             WEBVIEWINTERNAL_GETZOOMMODE)

  WebViewInternalGetZoomModeFunction();

 protected:
  ~WebViewInternalGetZoomModeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetZoomModeFunction);
};

class WebViewInternalFindFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.find", WEBVIEWINTERNAL_FIND)

  WebViewInternalFindFunction();

  // Used by WebViewInternalFindHelper to Respond().
  void ForwardResponse(const base::DictionaryValue& results);

 protected:
  ~WebViewInternalFindFunction() override;

 private:
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalFindFunction);
};

class WebViewInternalStopFindingFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.stopFinding",
                             WEBVIEWINTERNAL_STOPFINDING)

  WebViewInternalStopFindingFunction();

 protected:
  ~WebViewInternalStopFindingFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalStopFindingFunction);
};

class WebViewInternalLoadDataWithBaseUrlFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.loadDataWithBaseUrl",
                             WEBVIEWINTERNAL_LOADDATAWITHBASEURL)

  WebViewInternalLoadDataWithBaseUrlFunction();

 protected:
  ~WebViewInternalLoadDataWithBaseUrlFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalLoadDataWithBaseUrlFunction);
};

class WebViewInternalGoFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.go", WEBVIEWINTERNAL_GO)

  WebViewInternalGoFunction();

 protected:
  ~WebViewInternalGoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGoFunction);
};

class WebViewInternalReloadFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.reload", WEBVIEWINTERNAL_RELOAD)

  WebViewInternalReloadFunction();

 protected:
  ~WebViewInternalReloadFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalReloadFunction);
};

class WebViewInternalSetPermissionFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setPermission",
                             WEBVIEWINTERNAL_SETPERMISSION)

  WebViewInternalSetPermissionFunction();

 protected:
  ~WebViewInternalSetPermissionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetPermissionFunction);
};

class WebViewInternalOverrideUserAgentFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.overrideUserAgent",
                             WEBVIEWINTERNAL_OVERRIDEUSERAGENT)

  WebViewInternalOverrideUserAgentFunction();

 protected:
  ~WebViewInternalOverrideUserAgentFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalOverrideUserAgentFunction);
};

class WebViewInternalStopFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.stop", WEBVIEWINTERNAL_STOP)

  WebViewInternalStopFunction();

 protected:
  ~WebViewInternalStopFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalStopFunction);
};

class WebViewInternalSetAudioMutedFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAudioMuted",
                             WEBVIEWINTERNAL_SETAUDIOMUTED)

  WebViewInternalSetAudioMutedFunction();

 protected:
  ~WebViewInternalSetAudioMutedFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetAudioMutedFunction);
};

class WebViewInternalIsAudioMutedFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.isAudioMuted",
                             WEBVIEWINTERNAL_ISAUDIOMUTED)

  WebViewInternalIsAudioMutedFunction();

 protected:
  ~WebViewInternalIsAudioMutedFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalIsAudioMutedFunction);
};

class WebViewInternalGetAudioStateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getAudioState",
                             WEBVIEWINTERNAL_GETAUDIOSTATE)

  WebViewInternalGetAudioStateFunction();

 protected:
  ~WebViewInternalGetAudioStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetAudioStateFunction);
};

class WebViewInternalTerminateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.terminate",
                             WEBVIEWINTERNAL_TERMINATE)

  WebViewInternalTerminateFunction();

 protected:
  ~WebViewInternalTerminateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalTerminateFunction);
};

class WebViewInternalClearDataFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.clearData",
                             WEBVIEWINTERNAL_CLEARDATA)

  WebViewInternalClearDataFunction();

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

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalClearDataFunction);
};

class WebViewInternalSetSpatialNavigationEnabledFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setSpatialNavigationEnabled",
                             WEBVIEWINTERNAL_SETSPATIALNAVIGATIONENABLED)

  WebViewInternalSetSpatialNavigationEnabledFunction();

 protected:
  ~WebViewInternalSetSpatialNavigationEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetSpatialNavigationEnabledFunction);
};

class WebViewInternalIsSpatialNavigationEnabledFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.isSpatialNavigationEnabled",
                             WEBVIEWINTERNAL_ISSPATIALNAVIGATIONENABLED)

  WebViewInternalIsSpatialNavigationEnabledFunction();

 protected:
  ~WebViewInternalIsSpatialNavigationEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalIsSpatialNavigationEnabledFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_GUEST_VIEW_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_
