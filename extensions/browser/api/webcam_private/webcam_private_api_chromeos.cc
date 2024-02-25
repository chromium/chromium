// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/webcam_private_api.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/api/serial/serial_port_manager.h"
#include "extensions/browser/api/webcam_private/ip_webcam.h"
#include "extensions/browser/api/webcam_private/v4l2_webcam.h"
#include "extensions/browser/api/webcam_private/visca_webcam.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "url/origin.h"

namespace webcam_private = extensions::api::webcam_private;

namespace content {
class BrowserContext;
}  // namespace content

namespace {

const char kPathInUse[] = "Path in use";
const char kUnknownWebcam[] = "Unknown webcam id";
const char kOpenSerialWebcamError[] = "Can't open serial webcam.";
const char kGetWebcamPTZError[] = "Can't get web camera pan/tilt/zoom.";
const char kSetWebcamPTZError[] = "Can't set web camera pan/tilt/zoom.";
const char kResetWebcamError[] = "Can't reset web camera.";
const char kSetHomeWebcamError[] = "Can't set home position";
const char kRestorePresetWebcamError[] = "Can't restore preset.";
const char kSetPresetWebcamError[] = "Can't set preset.";

}  // namespace

namespace extensions {

// static
WebcamPrivateAPI* WebcamPrivateAPI::Get(content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

WebcamPrivateAPI::WebcamPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  webcam_resource_manager_ =
      std::make_unique<ApiResourceManager<WebcamResource>>(context);
}

WebcamPrivateAPI::~WebcamPrivateAPI() {
}

void WebcamPrivateAPI::OnGotDeviceIdOnUIThread(
    const ExtensionId& extension_id,
    const std::string& webcam_id,
    base::OnceCallback<void(Webcam*)> callback,
    const std::optional<std::string>& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!device_id) {
    std::move(callback).Run(nullptr);
    return;
  }

  Webcam* webcam = nullptr;

  if (device_id->compare(0, 8, "192.168.") == 0) {
    webcam = new IpWebcam(device_id.value());
  } else {
    V4L2Webcam* v4l2_webcam = new V4L2Webcam(device_id.value());
    if (!v4l2_webcam->Open()) {
      std::move(callback).Run(nullptr);
      return;
    }
    webcam = v4l2_webcam;
  }

  webcam_resource_manager_->Add(
      new WebcamResource(extension_id, webcam, webcam_id));

  std::move(callback).Run(webcam);
}

// static
void WebcamPrivateAPI::GetDeviceIdOnIOThread(
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::GetMediaDeviceIDForHMAC(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt,
      std::move(security_origin), std::move(hmac_device_id),
      content::GetUIThreadTaskRunner({}), std::move(callback));
}

void WebcamPrivateAPI::GetWebcam(const ExtensionId& extension_id,
                                 const std::string& webcam_id,
                                 base::OnceCallback<void(Webcam*)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebcamResource* webcam_resource = FindWebcamResource(extension_id, webcam_id);
  if (webcam_resource) {
    Webcam* webcam = webcam_resource->GetWebcam();
    std::move(callback).Run(webcam);
    return;
  }

  url::Origin security_origin =
      extensions::Extension::CreateOriginFromExtensionId(extension_id);
  if (media_device_salt::MediaDeviceSaltService* salt_service =
          ExtensionsBrowserClient::Get()->GetMediaDeviceSaltService(
              browser_context_)) {
    salt_service->GetSalt(
        blink::StorageKey::CreateFirstParty(security_origin),
        base::BindOnce(&WebcamPrivateAPI::GetDeviceIdOnUIThread,
                       weak_ptr_factory_.GetWeakPtr(), security_origin,
                       extension_id, webcam_id, std::move(callback)));
  } else {
    // If the embedder does not provide a salt service, use the browser
    // context's unique ID as salt.
    GetDeviceIdOnUIThread(security_origin, extension_id, webcam_id,
                          std::move(callback), browser_context_->UniqueId());
  }
}

void WebcamPrivateAPI::GetDeviceIdOnUIThread(
    const url::Origin& security_origin,
    const ExtensionId& extension_id,
    const std::string& webcam_id,
    base::OnceCallback<void(Webcam*)> webcam_callback,
    const std::string& salt) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto got_device_cb =
      base::BindOnce(&WebcamPrivateAPI::OnGotDeviceIdOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), extension_id, webcam_id,
                     std::move(webcam_callback));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebcamPrivateAPI::GetDeviceIdOnIOThread, salt,
                     security_origin, webcam_id, std::move(got_device_cb)));
}

void WebcamPrivateAPI::OpenSerialWebcam(
    const ExtensionId& extension_id,
    const std::string& device_path,
    const base::RepeatingCallback<void(const std::string&,
                                       OpenSerialWebcamResult)>& callback) {
  GetWebcamId(extension_id, device_path,
              base::BindOnce(&WebcamPrivateAPI::GotWebcamId,
                             weak_ptr_factory_.GetWeakPtr(), extension_id,
                             device_path, callback));
}

void WebcamPrivateAPI::GotWebcamId(
    const ExtensionId& extension_id,
    const std::string& device_path,
    const base::RepeatingCallback<void(const std::string&,
                                       OpenSerialWebcamResult)>& callback,
    const std::string& webcam_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebcamResource* webcam_resource = FindWebcamResource(extension_id, webcam_id);
  if (webcam_resource) {
    callback.Run("", OpenSerialWebcamResult::kInUse);
    return;
  }

  mojo::PendingRemote<device::mojom::SerialPort> port;
  auto* port_manager = api::SerialPortManager::Get(browser_context_);
  DCHECK(port_manager);

  auto visca_webcam = base::MakeRefCounted<ViscaWebcam>();
  visca_webcam->Open(
      extension_id, port_manager, device_path,
      base::BindRepeating(&WebcamPrivateAPI::OnOpenSerialWebcam,
                          weak_ptr_factory_.GetWeakPtr(), webcam_id,
                          extension_id, device_path, visca_webcam, callback));
}

bool WebcamPrivateAPI::CloseWebcam(const ExtensionId& extension_id,
                                   const std::string& webcam_id) {
  if (FindWebcamResource(extension_id, webcam_id)) {
    RemoveWebcamResource(extension_id, webcam_id);
    return true;
  }
  return false;
}

void WebcamPrivateAPI::OnOpenSerialWebcam(
    const std::string& webcam_id,
    const ExtensionId& extension_id,
    const std::string& device_path,
    scoped_refptr<Webcam> webcam,
    const base::RepeatingCallback<void(const std::string&,
                                       OpenSerialWebcamResult)>& callback,
    bool success) {
  if (success) {
    webcam_resource_manager_->Add(
        new WebcamResource(extension_id, webcam.get(), webcam_id));
    callback.Run(webcam_id, OpenSerialWebcamResult::kSuccess);
  } else {
    callback.Run("", OpenSerialWebcamResult::kError);
  }
}

void WebcamPrivateAPI::GetWebcamId(
    const ExtensionId& extension_id,
    const std::string& device_id,
    base::OnceCallback<void(const std::string&)> webcam_id_callback) {
  url::Origin security_origin =
      extensions::Extension::CreateOriginFromExtensionId(extension_id);
  if (media_device_salt::MediaDeviceSaltService* salt_service =
          ExtensionsBrowserClient::Get()->GetMediaDeviceSaltService(
              browser_context_)) {
    salt_service->GetSalt(
        blink::StorageKey::CreateFirstParty(security_origin),
        base::BindOnce(&WebcamPrivateAPI::FinalizeGetWebcamId,
                       weak_ptr_factory_.GetWeakPtr(), security_origin,
                       device_id, std::move(webcam_id_callback)));
  } else {
    // If the embedder does not provide a salt service, use the browser
    // context's unique ID as salt.
    FinalizeGetWebcamId(security_origin, device_id,
                        std::move(webcam_id_callback),
                        browser_context_->UniqueId());
  }
}

void WebcamPrivateAPI::FinalizeGetWebcamId(
    const url::Origin& security_origin,
    const std::string& device_id,
    base::OnceCallback<void(const std::string&)> webcam_id_callback,
    const std::string& device_id_salt) {
  std::string webcam_id = content::GetHMACForMediaDeviceID(
      device_id_salt, security_origin, device_id);
  std::move(webcam_id_callback).Run(webcam_id);
}

WebcamResource* WebcamPrivateAPI::FindWebcamResource(
    const ExtensionId& extension_id,
    const std::string& webcam_id) const {
  DCHECK(webcam_resource_manager_);

  std::unordered_set<int>* connection_ids =
      webcam_resource_manager_->GetResourceIds(extension_id);
  if (!connection_ids)
    return nullptr;

  for (const auto& connection_id : *connection_ids) {
    WebcamResource* webcam_resource =
        webcam_resource_manager_->Get(extension_id, connection_id);
    if (webcam_resource && webcam_resource->GetWebcamId() == webcam_id)
      return webcam_resource;
  }

  return nullptr;
}

bool WebcamPrivateAPI::RemoveWebcamResource(const ExtensionId& extension_id,
                                            const std::string& webcam_id) {
  DCHECK(webcam_resource_manager_);

  std::unordered_set<int>* connection_ids =
      webcam_resource_manager_->GetResourceIds(extension_id);
  if (!connection_ids)
    return false;

  for (const auto& connection_id : *connection_ids) {
    WebcamResource* webcam_resource =
        webcam_resource_manager_->Get(extension_id, connection_id);
    if (webcam_resource && webcam_resource->GetWebcamId() == webcam_id) {
      webcam_resource_manager_->Remove(extension_id, connection_id);
      return true;
    }
  }

  return false;
}

WebcamPrivateOpenSerialWebcamFunction::WebcamPrivateOpenSerialWebcamFunction() {
}

WebcamPrivateOpenSerialWebcamFunction::
    ~WebcamPrivateOpenSerialWebcamFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateOpenSerialWebcamFunction::Run() {
  std::optional<webcam_private::OpenSerialWebcam::Params> params =
      webcam_private::OpenSerialWebcam::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WebcamPrivateAPI::Get(browser_context())
      ->OpenSerialWebcam(
          extension_id(), params->path,
          base::BindRepeating(
              &WebcamPrivateOpenSerialWebcamFunction::OnOpenWebcam, this));
  // OpenSerialWebcam responds asynchronously.
  return RespondLater();
}

void WebcamPrivateOpenSerialWebcamFunction::OnOpenWebcam(
    const std::string& webcam_id,
    WebcamPrivateAPI::OpenSerialWebcamResult result) {
  if (result == WebcamPrivateAPI::OpenSerialWebcamResult::kSuccess) {
    Respond(WithArguments(webcam_id));
  } else if (result == WebcamPrivateAPI::OpenSerialWebcamResult::kError) {
    Respond(Error(kOpenSerialWebcamError));
  } else if (result == WebcamPrivateAPI::OpenSerialWebcamResult::kInUse) {
    Respond(Error(kPathInUse));
  }
}

WebcamPrivateCloseWebcamFunction::WebcamPrivateCloseWebcamFunction() {
}

WebcamPrivateCloseWebcamFunction::~WebcamPrivateCloseWebcamFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateCloseWebcamFunction::Run() {
  std::optional<webcam_private::CloseWebcam::Params> params =
      webcam_private::CloseWebcam::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool success = WebcamPrivateAPI::Get(browser_context())
                           ->CloseWebcam(extension_id(), params->webcam_id);
  return RespondNow(success ? NoArguments() : Error(kUnknownErrorDoNotUse));
}

WebcamPrivateSetFunction::WebcamPrivateSetFunction() {
}

WebcamPrivateSetFunction::~WebcamPrivateSetFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateSetFunction::Run() {
  std::optional<webcam_private::Set::Params> params =
      webcam_private::Set::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string webcam_id = params->webcam_id;

  auto on_webcam = base::BindOnce(&WebcamPrivateSetFunction::OnWebcam, this,
                                  std::move(params));

  WebcamPrivateAPI::Get(browser_context())
      ->GetWebcam(extension_id(), webcam_id, std::move(on_webcam));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WebcamPrivateSetFunction::OnWebcam(
    std::optional<webcam_private::Set::Params> params,
    Webcam* webcam) {
  if (!webcam)
    return Respond(Error(kUnknownWebcam));

  int pan_speed = 0;
  int tilt_speed = 0;
  if (params->config.pan_speed)
    pan_speed = *(params->config.pan_speed);

  if (params->config.tilt_speed)
    tilt_speed = *(params->config.tilt_speed);

  // Count all of the requests we will send before potentially sending any.
  pending_num_set_webcam_param_requests_ = 0;
  if (params->config.pan) {
    ++pending_num_set_webcam_param_requests_;
  }
  if (params->config.pan_direction != webcam_private::PanDirection::kNone) {
    ++pending_num_set_webcam_param_requests_;
  }
  if (params->config.tilt) {
    ++pending_num_set_webcam_param_requests_;
  }
  if (params->config.tilt_direction != webcam_private::TiltDirection::kNone) {
    ++pending_num_set_webcam_param_requests_;
  }
  if (params->config.zoom) {
    ++pending_num_set_webcam_param_requests_;
  }
  if (params->config.autofocus_state != webcam_private::AutofocusState::kNone) {
    ++pending_num_set_webcam_param_requests_;
  }
  if (params->config.focus) {
    ++pending_num_set_webcam_param_requests_;
  }

  if (params->config.pan) {
    webcam->SetPan(*(params->config.pan), pan_speed,
                   base::BindRepeating(
                       &WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.pan_direction != webcam_private::PanDirection::kNone) {
    Webcam::PanDirection direction = Webcam::PAN_STOP;
    switch (params->config.pan_direction) {
      case webcam_private::PanDirection::kNone:
      case webcam_private::PanDirection::kStop:
        direction = Webcam::PAN_STOP;
        break;

      case webcam_private::PanDirection::kRight:
        direction = Webcam::PAN_RIGHT;
        break;

      case webcam_private::PanDirection::kLeft:
        direction = Webcam::PAN_LEFT;
        break;
    }
    webcam->SetPanDirection(
        direction, pan_speed,
        base::BindRepeating(&WebcamPrivateSetFunction::OnSetWebcamParameters,
                            this));
  }

  if (params->config.tilt) {
    webcam->SetTilt(
        *(params->config.tilt), tilt_speed,
        base::BindRepeating(&WebcamPrivateSetFunction::OnSetWebcamParameters,
                            this));
  }

  if (params->config.tilt_direction != webcam_private::TiltDirection::kNone) {
    Webcam::TiltDirection direction = Webcam::TILT_STOP;
    switch (params->config.tilt_direction) {
      case webcam_private::TiltDirection::kNone:
      case webcam_private::TiltDirection::kStop:
        direction = Webcam::TILT_STOP;
        break;

      case webcam_private::TiltDirection::kUp:
        direction = Webcam::TILT_UP;
        break;

      case webcam_private::TiltDirection::kDown:
        direction = Webcam::TILT_DOWN;
        break;
    }
    webcam->SetTiltDirection(
        direction, tilt_speed,
        base::BindRepeating(&WebcamPrivateSetFunction::OnSetWebcamParameters,
                            this));
  }

  if (params->config.zoom) {
    webcam->SetZoom(
        *(params->config.zoom),
        base::BindRepeating(&WebcamPrivateSetFunction::OnSetWebcamParameters,
                            this));
  }

  if (params->config.autofocus_state != webcam_private::AutofocusState::kNone) {
    Webcam::AutofocusState state = Webcam::AUTOFOCUS_ON;
    switch (params->config.autofocus_state) {
      case webcam_private::AutofocusState::kNone:
      case webcam_private::AutofocusState::kOff:
        state = Webcam::AUTOFOCUS_OFF;
        break;

      case webcam_private::AutofocusState::kOn:
        state = Webcam::AUTOFOCUS_ON;
        break;
    }
    webcam->SetAutofocusState(
        state, base::BindRepeating(
                   &WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.focus) {
    webcam->SetFocus(
        *(params->config.focus),
        base::BindRepeating(&WebcamPrivateSetFunction::OnSetWebcamParameters,
                            this));
  }
}

void WebcamPrivateSetFunction::OnSetWebcamParameters(bool success) {
  failed_ |= !success;
  --pending_num_set_webcam_param_requests_;

  DCHECK_GE(pending_num_set_webcam_param_requests_, 0);
  if (pending_num_set_webcam_param_requests_ != 0) {
    return;
  }

  if (failed_) {
    Respond(Error(kSetWebcamPTZError));
    return;
  }

  // Reply with a dummy, empty configuration.
  webcam_private::WebcamCurrentConfiguration result;
  Respond(WithArguments(result.ToValue()));
}

WebcamPrivateGetFunction::WebcamPrivateGetFunction()
    : min_pan_(0),
      max_pan_(0),
      pan_(0),
      min_tilt_(0),
      max_tilt_(0),
      tilt_(0),
      min_zoom_(0),
      max_zoom_(0),
      zoom_(0),
      min_focus_(0),
      max_focus_(0),
      focus_(0),
      got_pan_(false),
      got_tilt_(false),
      got_zoom_(false),
      got_focus_(false),
      success_(false) {}

WebcamPrivateGetFunction::~WebcamPrivateGetFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateGetFunction::Run() {
  std::optional<webcam_private::Get::Params> params =
      webcam_private::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto on_webcam = base::BindOnce(&WebcamPrivateGetFunction::OnWebcam, this);

  WebcamPrivateAPI::Get(browser_context())
      ->GetWebcam(extension_id(), params->webcam_id, std::move(on_webcam));

  // Might have already responded if webcam_resource_manager_ already has the
  // Webcam.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WebcamPrivateGetFunction::OnWebcam(Webcam* webcam) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!webcam) {
    Respond(Error(kUnknownWebcam));
    return;
  }

  webcam->GetPan(base::BindRepeating(
      &WebcamPrivateGetFunction::OnGetWebcamParameters, this, INQUIRY_PAN));
  webcam->GetTilt(base::BindRepeating(
      &WebcamPrivateGetFunction::OnGetWebcamParameters, this, INQUIRY_TILT));
  webcam->GetZoom(base::BindRepeating(
      &WebcamPrivateGetFunction::OnGetWebcamParameters, this, INQUIRY_ZOOM));
  webcam->GetFocus(base::BindRepeating(
      &WebcamPrivateGetFunction::OnGetWebcamParameters, this, INQUIRY_FOCUS));
}

// Retrieve webcam parameters. Will respond a config holding the requested
// values if any of the requests succeeds. Otherwise will respond an error.
void WebcamPrivateGetFunction::OnGetWebcamParameters(InquiryType type,
                                                     bool success,
                                                     int value,
                                                     int min_value,
                                                     int max_value) {
  success_ = success_ || success;

  switch (type) {
    case INQUIRY_PAN:
      if (success) {
        min_pan_ = min_value;
        max_pan_ = max_value;
        pan_ = value;
      }
      got_pan_ = true;
      break;
    case INQUIRY_TILT:
      if (success) {
        min_tilt_ = min_value;
        max_tilt_ = max_value;
        tilt_ = value;
      }
      got_tilt_ = true;
      break;
    case INQUIRY_ZOOM:
      if (success) {
        min_zoom_ = min_value;
        max_zoom_ = max_value;
        zoom_ = value;
      }
      got_zoom_ = true;
      break;
    case INQUIRY_FOCUS:
      if (success) {
        min_focus_ = min_value;
        max_focus_ = max_value;
        focus_ = value;
      }
      got_focus_ = true;
      break;
  }
  if (got_pan_ && got_tilt_ && got_zoom_ && got_focus_) {
    if (!success_) {
      Respond(Error(kGetWebcamPTZError));
      return;
    }

    webcam_private::WebcamCurrentConfiguration result;
    if (min_pan_ != max_pan_) {
      result.pan_range.emplace();
      result.pan_range->min = min_pan_;
      result.pan_range->max = max_pan_;
    }
    if (min_tilt_ != max_tilt_) {
      result.tilt_range.emplace();
      result.tilt_range->min = min_tilt_;
      result.tilt_range->max = max_tilt_;
    }
    if (min_zoom_ != max_zoom_) {
      result.zoom_range.emplace();
      result.zoom_range->min = min_zoom_;
      result.zoom_range->max = max_zoom_;
    }
    if (min_focus_ != max_focus_) {
      result.focus_range.emplace();
      result.focus_range->min = min_focus_;
      result.focus_range->max = max_focus_;
    }

    result.pan = pan_;
    result.tilt = tilt_;
    result.zoom = zoom_;
    result.focus = focus_;
    Respond(WithArguments(result.ToValue()));
  }
}

WebcamPrivateResetFunction::WebcamPrivateResetFunction() {
}

WebcamPrivateResetFunction::~WebcamPrivateResetFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateResetFunction::Run() {
  std::optional<webcam_private::Reset::Params> params =
      webcam_private::Reset::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string webcam_id = params->webcam_id;

  auto on_webcam = base::BindOnce(&WebcamPrivateResetFunction::OnWebcam, this,
                                  std::move(params));

  WebcamPrivateAPI::Get(browser_context())
      ->GetWebcam(extension_id(), webcam_id, std::move(on_webcam));

  // Might have already responsed if webcam_resource_manager_ already has the
  // Webcam and WebCam::Reset just runs the callback.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WebcamPrivateResetFunction::OnWebcam(
    std::optional<webcam_private::Reset::Params> params,
    Webcam* webcam) {
  if (!webcam)
    return Respond(Error(kUnknownWebcam));

  webcam->Reset(
      params->config.pan.has_value(), params->config.tilt.has_value(),
      params->config.zoom.has_value(),
      base::BindRepeating(&WebcamPrivateResetFunction::OnResetWebcam, this));
}

void WebcamPrivateResetFunction::OnResetWebcam(bool success) {
  if (!success) {
    Respond(Error(kResetWebcamError));
    return;
  }

  // Reply with a dummy, empty configuration.
  webcam_private::WebcamCurrentConfiguration result;
  Respond(WithArguments(result.ToValue()));
}

WebcamPrivateSetHomeFunction::WebcamPrivateSetHomeFunction() = default;

WebcamPrivateSetHomeFunction::~WebcamPrivateSetHomeFunction() = default;

ExtensionFunction::ResponseAction WebcamPrivateSetHomeFunction::Run() {
  std::optional<webcam_private::SetHome::Params> params =
      webcam_private::SetHome::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto on_webcam =
      base::BindOnce(&WebcamPrivateSetHomeFunction::OnWebcam, this);

  WebcamPrivateAPI::Get(browser_context())
      ->GetWebcam(extension_id(), params->webcam_id, std::move(on_webcam));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WebcamPrivateSetHomeFunction::OnWebcam(Webcam* webcam) {
  if (!webcam)
    return Respond(Error(kUnknownWebcam));

  webcam->SetHome(base::BindRepeating(
      &WebcamPrivateSetHomeFunction::OnSetHomeWebcam, this));
}

void WebcamPrivateSetHomeFunction::OnSetHomeWebcam(bool success) {
  if (!success) {
    Respond(Error(kSetHomeWebcamError));
    return;
  }

  // Reply with a dummy, empty configuration.
  webcam_private::WebcamCurrentConfiguration result;
  Respond(WithArguments(result.ToValue()));
}

WebcamPrivateRestoreCameraPresetFunction::
    WebcamPrivateRestoreCameraPresetFunction() {}

WebcamPrivateRestoreCameraPresetFunction::
    ~WebcamPrivateRestoreCameraPresetFunction() {}

ExtensionFunction::ResponseAction
WebcamPrivateRestoreCameraPresetFunction::Run() {
  std::optional<webcam_private::RestoreCameraPreset::Params> params =
      webcam_private::RestoreCameraPreset::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto on_webcam =
      base::BindOnce(&WebcamPrivateRestoreCameraPresetFunction::OnWebcam, this,
                     params->preset_number);

  WebcamPrivateAPI::Get(browser_context())
      ->GetWebcam(extension_id(), params->webcam_id, std::move(on_webcam));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WebcamPrivateRestoreCameraPresetFunction::OnWebcam(int preset_number,
                                                        Webcam* webcam) {
  if (!webcam) {
    Respond(Error(kUnknownWebcam));
    return;
  }

  webcam->RestoreCameraPreset(
      preset_number,
      base::BindRepeating(&WebcamPrivateRestoreCameraPresetFunction::
                              OnRestoreCameraPresetWebcam,
                          this));
}

void WebcamPrivateRestoreCameraPresetFunction::OnRestoreCameraPresetWebcam(
    bool success) {
  if (!success) {
    Respond(Error(kRestorePresetWebcamError));
    return;
  }

  // Reply with a dummy, empty configuration.
  webcam_private::WebcamCurrentConfiguration result;
  Respond(WithArguments(result.ToValue()));
}

WebcamPrivateSetCameraPresetFunction::WebcamPrivateSetCameraPresetFunction() =
    default;

WebcamPrivateSetCameraPresetFunction::~WebcamPrivateSetCameraPresetFunction() =
    default;

ExtensionFunction::ResponseAction WebcamPrivateSetCameraPresetFunction::Run() {
  std::optional<webcam_private::SetCameraPreset::Params> params =
      webcam_private::SetCameraPreset::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto on_webcam =
      base::BindOnce(&WebcamPrivateSetCameraPresetFunction::OnWebcam, this,
                     params->preset_number);

  WebcamPrivateAPI::Get(browser_context())
      ->GetWebcam(extension_id(), params->webcam_id, std::move(on_webcam));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WebcamPrivateSetCameraPresetFunction::OnWebcam(int preset_number,
                                                    Webcam* webcam) {
  if (!webcam) {
    Respond(Error(kUnknownWebcam));
    return;
  }

  webcam->SetCameraPreset(
      preset_number,
      base::BindRepeating(
          &WebcamPrivateSetCameraPresetFunction::OnSetCameraPresetWebcam,
          this));
}

void WebcamPrivateSetCameraPresetFunction::OnSetCameraPresetWebcam(
    bool success) {
  if (!success) {
    Respond(Error(kSetPresetWebcamError));
    return;
  }

  // Reply with a dummy, empty configuration.
  webcam_private::WebcamCurrentConfiguration result;
  Respond(WithArguments(result.ToValue()));
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<WebcamPrivateAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<WebcamPrivateAPI>*
WebcamPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<WebcamPrivateAPI>
    ::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ProcessManagerFactory::GetInstance());
}

}  // namespace extensions
