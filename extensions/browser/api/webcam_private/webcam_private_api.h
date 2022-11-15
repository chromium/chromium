// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_

#include <map>
#include <memory>

#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/webcam_private/webcam.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/api/webcam_private.h"

namespace extensions {

class WebcamPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<WebcamPrivateAPI>* GetFactoryInstance();

  // Convenience method to get the WebcamPrivateAPI for a BrowserContext.
  static WebcamPrivateAPI* Get(content::BrowserContext* context);

  explicit WebcamPrivateAPI(content::BrowserContext* context);

  WebcamPrivateAPI(const WebcamPrivateAPI&) = delete;
  WebcamPrivateAPI& operator=(const WebcamPrivateAPI&) = delete;

  ~WebcamPrivateAPI() override;

  void GetWebcam(const std::string& extension_id,
                 const std::string& webcam_id,
                 base::OnceCallback<void(Webcam*)> callback);

  bool OpenSerialWebcam(
      const std::string& extension_id,
      const std::string& device_path,
      const base::RepeatingCallback<void(const std::string&, bool)>& callback);
  bool CloseWebcam(const std::string& extension_id,
                   const std::string& device_id);

 private:
  friend class BrowserContextKeyedAPIFactory<WebcamPrivateAPI>;

  void OnGotDeviceIdOnUIThread(const std::string& extension_id,
                               const std::string& webcam_id,
                               base::OnceCallback<void(Webcam*)> callback,
                               const absl::optional<std::string>& device_id);

  static void GetDeviceIdOnIOThread(
      std::string salt,
      url::Origin security_origin,
      std::string hmac_device_id,
      base::OnceCallback<void(const absl::optional<std::string>&)> callback);

  void OnOpenSerialWebcam(
      const std::string& extension_id,
      const std::string& device_path,
      scoped_refptr<Webcam> webcam,
      const base::RepeatingCallback<void(const std::string&, bool)>& callback,
      bool success);

  std::string GetWebcamId(const std::string& extension_id,
                          const std::string& device_id);

  WebcamResource* FindWebcamResource(const std::string& extension_id,
                                     const std::string& webcam_id) const;
  bool RemoveWebcamResource(const std::string& extension_id,
                            const std::string& webcam_id);

  // BrowserContextKeyedAPI:
  static const char* service_name() {
    return "WebcamPrivateAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  content::BrowserContext* const browser_context_;
  std::unique_ptr<ApiResourceManager<WebcamResource>> webcam_resource_manager_;

  base::WeakPtrFactory<WebcamPrivateAPI> weak_ptr_factory_{this};
};

template <>
void BrowserContextKeyedAPIFactory<WebcamPrivateAPI>
    ::DeclareFactoryDependencies();

class WebcamPrivateOpenSerialWebcamFunction : public ExtensionFunction {
 public:
  WebcamPrivateOpenSerialWebcamFunction();

  WebcamPrivateOpenSerialWebcamFunction(
      const WebcamPrivateOpenSerialWebcamFunction&) = delete;
  WebcamPrivateOpenSerialWebcamFunction& operator=(
      const WebcamPrivateOpenSerialWebcamFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.openSerialWebcam",
                             WEBCAMPRIVATE_OPENSERIALWEBCAM)

 protected:
  ~WebcamPrivateOpenSerialWebcamFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnOpenWebcam(const std::string& webcam_id, bool success);
};

class WebcamPrivateCloseWebcamFunction : public ExtensionFunction {
 public:
  WebcamPrivateCloseWebcamFunction();

  WebcamPrivateCloseWebcamFunction(const WebcamPrivateCloseWebcamFunction&) =
      delete;
  WebcamPrivateCloseWebcamFunction& operator=(
      const WebcamPrivateCloseWebcamFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.closeWebcam",
                             WEBCAMPRIVATE_CLOSEWEBCAM)

 protected:
  ~WebcamPrivateCloseWebcamFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class WebcamPrivateSetFunction : public ExtensionFunction {
 public:
  WebcamPrivateSetFunction();

  WebcamPrivateSetFunction(const WebcamPrivateSetFunction&) = delete;
  WebcamPrivateSetFunction& operator=(const WebcamPrivateSetFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.set", WEBCAMPRIVATE_SET)

 protected:
  ~WebcamPrivateSetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnWebcam(
      std::unique_ptr<extensions::api::webcam_private::Set::Params> params,
      Webcam* webcam);
  void OnSetWebcamParameters(bool success);

  int pending_num_set_webcam_param_requests_ = 0;
  bool failed_ = false;
};

class WebcamPrivateGetFunction : public ExtensionFunction {
 public:
  WebcamPrivateGetFunction();

  WebcamPrivateGetFunction(const WebcamPrivateGetFunction&) = delete;
  WebcamPrivateGetFunction& operator=(const WebcamPrivateGetFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.get", WEBCAMPRIVATE_GET)

 protected:
  ~WebcamPrivateGetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  enum InquiryType {
    INQUIRY_PAN,
    INQUIRY_TILT,
    INQUIRY_ZOOM,
    INQUIRY_FOCUS,
  };

  enum AutofocusState {
    AUTOFOCUSSTATE_ON,
    AUTOFOCUSSTATE_OFF,
  };

  void OnWebcam(Webcam* webcam);
  void OnGetWebcamParameters(InquiryType type,
                             bool success,
                             int value,
                             int min_value,
                             int max_value);

  int min_pan_;
  int max_pan_;
  int pan_;
  int min_tilt_;
  int max_tilt_;
  int tilt_;
  int min_zoom_;
  int max_zoom_;
  int zoom_;
  int min_focus_;
  int max_focus_;
  int focus_;
  bool got_pan_;
  bool got_tilt_;
  bool got_zoom_;
  bool got_focus_;
  bool success_;
};

class WebcamPrivateResetFunction : public ExtensionFunction {
 public:
  WebcamPrivateResetFunction();

  WebcamPrivateResetFunction(const WebcamPrivateResetFunction&) = delete;
  WebcamPrivateResetFunction& operator=(const WebcamPrivateResetFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.reset", WEBCAMPRIVATE_RESET)

 protected:
  ~WebcamPrivateResetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnWebcam(
      std::unique_ptr<extensions::api::webcam_private::Reset::Params> params,
      Webcam* webcam);
  void OnResetWebcam(bool success);
};

class WebcamPrivateSetHomeFunction : public ExtensionFunction {
 public:
  WebcamPrivateSetHomeFunction();

  WebcamPrivateSetHomeFunction(const WebcamPrivateSetHomeFunction&) = delete;
  WebcamPrivateSetHomeFunction& operator=(const WebcamPrivateSetHomeFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.setHome", WEBCAMPRIVATE_SET_HOME)

 protected:
  ~WebcamPrivateSetHomeFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnWebcam(Webcam* webcam);
  void OnSetHomeWebcam(bool success);
};

class WebcamPrivateRestoreCameraPresetFunction : public ExtensionFunction {
 public:
  WebcamPrivateRestoreCameraPresetFunction();

  WebcamPrivateRestoreCameraPresetFunction(
      const WebcamPrivateRestoreCameraPresetFunction&) = delete;
  WebcamPrivateRestoreCameraPresetFunction& operator=(
      const WebcamPrivateRestoreCameraPresetFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.restoreCameraPreset",
                             WEBCAMPRIVATE_RESTORE_CAMERA_PRESET)

 protected:
  ~WebcamPrivateRestoreCameraPresetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnWebcam(int preset_number, Webcam* webcam);
  void OnRestoreCameraPresetWebcam(bool success);
};

class WebcamPrivateSetCameraPresetFunction : public ExtensionFunction {
 public:
  WebcamPrivateSetCameraPresetFunction();

  WebcamPrivateSetCameraPresetFunction(
      const WebcamPrivateSetCameraPresetFunction&) = delete;
  WebcamPrivateSetCameraPresetFunction& operator=(
      const WebcamPrivateSetCameraPresetFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("webcamPrivate.setCameraPreset",
                             WEBCAMPRIVATE_SET_CAMERA_PRESET)

 protected:
  ~WebcamPrivateSetCameraPresetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnWebcam(int preset_number, Webcam* webcam);
  void OnSetCameraPresetWebcam(bool success);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
