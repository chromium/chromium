// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/webcam_private/webcam.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/process_manager_observer.h"

namespace extensions {

class WebcamPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<WebcamPrivateAPI>* GetFactoryInstance();

  // Convenience method to get the WebcamPrivateAPI for a BrowserContext.
  static WebcamPrivateAPI* Get(content::BrowserContext* context);

  explicit WebcamPrivateAPI(content::BrowserContext* context);
  ~WebcamPrivateAPI() override;

  Webcam* GetWebcam(const std::string& extension_id,
                    const std::string& device_id);

  bool OpenSerialWebcam(
      const std::string& extension_id,
      const std::string& device_path,
      const base::Callback<void(const std::string&, bool)>& callback);
  bool CloseWebcam(const std::string& extension_id,
                   const std::string& device_id);

 private:
  friend class BrowserContextKeyedAPIFactory<WebcamPrivateAPI>;

  void OnOpenSerialWebcam(
      const std::string& extension_id,
      const std::string& device_path,
      scoped_refptr<Webcam> webcam,
      const base::Callback<void(const std::string&, bool)>& callback,
      bool success);

  // Note: This function does not work for serial devices. Do not use this
  // function for serial devices.
  bool GetDeviceId(const std::string& extension_id,
                   const std::string& webcam_id,
                   std::string* device_id);
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

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateAPI);
};

template <>
void BrowserContextKeyedAPIFactory<WebcamPrivateAPI>
    ::DeclareFactoryDependencies();

class WebcamPrivateOpenSerialWebcamFunction : public ExtensionFunction {
 public:
  WebcamPrivateOpenSerialWebcamFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.openSerialWebcam",
                             WEBCAMPRIVATE_OPENSERIALWEBCAM)

 protected:
  ~WebcamPrivateOpenSerialWebcamFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnOpenWebcam(const std::string& webcam_id, bool success);

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateOpenSerialWebcamFunction);
};

class WebcamPrivateCloseWebcamFunction : public ExtensionFunction {
 public:
  WebcamPrivateCloseWebcamFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.closeWebcam",
                             WEBCAMPRIVATE_CLOSEWEBCAM)

 protected:
  ~WebcamPrivateCloseWebcamFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateCloseWebcamFunction);
};

class WebcamPrivateSetFunction : public ExtensionFunction {
 public:
  WebcamPrivateSetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.set", WEBCAMPRIVATE_SET)

 protected:
  ~WebcamPrivateSetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnSetWebcamParameters(bool success);

  int pending_num_set_webcam_param_requests_ = 0;
  bool failed_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateSetFunction);
};

class WebcamPrivateGetFunction : public ExtensionFunction {
 public:
  WebcamPrivateGetFunction();
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

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateGetFunction);
};

class WebcamPrivateResetFunction : public ExtensionFunction {
 public:
  WebcamPrivateResetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.reset", WEBCAMPRIVATE_RESET)

 protected:
  ~WebcamPrivateResetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnResetWebcam(bool success);

  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateResetFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
