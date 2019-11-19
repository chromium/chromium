// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/webcam_private_api.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/resource_context.h"
#include "extensions/browser/api/serial/serial_port_manager.h"
#include "extensions/browser/api/webcam_private/v4l2_webcam.h"
#include "extensions/browser/api/webcam_private/visca_webcam.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/api/webcam_private.h"
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

}  // namespace

namespace extensions {

// static
WebcamPrivateAPI* WebcamPrivateAPI::Get(content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

WebcamPrivateAPI::WebcamPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  webcam_resource_manager_.reset(
      new ApiResourceManager<WebcamResource>(context));
}

WebcamPrivateAPI::~WebcamPrivateAPI() {
}

Webcam* WebcamPrivateAPI::GetWebcam(const std::string& extension_id,
                                    const std::string& webcam_id) {
  WebcamResource* webcam_resource = FindWebcamResource(extension_id, webcam_id);
  if (webcam_resource)
    return webcam_resource->GetWebcam();

  std::string device_id;
  GetDeviceId(extension_id, webcam_id, &device_id);
  V4L2Webcam* v4l2_webcam(new V4L2Webcam(device_id));
  if (!v4l2_webcam->Open()) {
    return nullptr;
  }

  webcam_resource_manager_->Add(
      new WebcamResource(extension_id, v4l2_webcam, webcam_id));

  return v4l2_webcam;
}

bool WebcamPrivateAPI::OpenSerialWebcam(
    const std::string& extension_id,
    const std::string& device_path,
    const base::Callback<void(const std::string&, bool)>& callback) {
  const std::string& webcam_id = GetWebcamId(extension_id, device_path);
  WebcamResource* webcam_resource = FindWebcamResource(extension_id, webcam_id);
  if (webcam_resource)
    return false;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<device::mojom::SerialPort> port;
  auto* port_manager = api::SerialPortManager::Get(browser_context_);
  DCHECK(port_manager);
  port_manager->GetPort(device_path, port.InitWithNewPipeAndPassReceiver());

  auto visca_webcam = base::MakeRefCounted<ViscaWebcam>();
  visca_webcam->Open(extension_id, std::move(port),
                     base::Bind(&WebcamPrivateAPI::OnOpenSerialWebcam,
                                weak_ptr_factory_.GetWeakPtr(), extension_id,
                                device_path, visca_webcam, callback));
  return true;
}

bool WebcamPrivateAPI::CloseWebcam(const std::string& extension_id,
                                   const std::string& webcam_id) {
  if (FindWebcamResource(extension_id, webcam_id)) {
    RemoveWebcamResource(extension_id, webcam_id);
    return true;
  }
  return false;
}

void WebcamPrivateAPI::OnOpenSerialWebcam(
    const std::string& extension_id,
    const std::string& device_path,
    scoped_refptr<Webcam> webcam,
    const base::Callback<void(const std::string&, bool)>& callback,
    bool success) {
  if (success) {
    const std::string& webcam_id = GetWebcamId(extension_id, device_path);
    webcam_resource_manager_->Add(
        new WebcamResource(extension_id, webcam.get(), webcam_id));
    callback.Run(webcam_id, true);
  } else {
    callback.Run("", false);
  }
}

bool WebcamPrivateAPI::GetDeviceId(const std::string& extension_id,
                                   const std::string& webcam_id,
                                   std::string* device_id) {
  url::Origin security_origin = url::Origin::Create(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id));

  return content::GetMediaDeviceIDForHMAC(
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      browser_context_->GetMediaDeviceIDSalt(), security_origin, webcam_id,
      device_id);
}

std::string WebcamPrivateAPI::GetWebcamId(const std::string& extension_id,
                                          const std::string& device_id) {
  url::Origin security_origin = url::Origin::Create(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id));

  return content::GetHMACForMediaDeviceID(
      browser_context_->GetMediaDeviceIDSalt(), security_origin, device_id);
}

WebcamResource* WebcamPrivateAPI::FindWebcamResource(
    const std::string& extension_id,
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

bool WebcamPrivateAPI::RemoveWebcamResource(const std::string& extension_id,
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
  std::unique_ptr<webcam_private::OpenSerialWebcam::Params> params(
      webcam_private::OpenSerialWebcam::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (WebcamPrivateAPI::Get(browser_context())
          ->OpenSerialWebcam(
              extension_id(), params->path,
              base::Bind(&WebcamPrivateOpenSerialWebcamFunction::OnOpenWebcam,
                         this))) {
    // OpenSerialWebcam responds asynchronously.
    return RespondLater();
  }

  return RespondNow(Error(kPathInUse));
}

void WebcamPrivateOpenSerialWebcamFunction::OnOpenWebcam(
    const std::string& webcam_id,
    bool success) {
  if (success) {
    Respond(OneArgument(std::make_unique<base::Value>(webcam_id)));
  } else {
    Respond(Error(kOpenSerialWebcamError));
  }
}

WebcamPrivateCloseWebcamFunction::WebcamPrivateCloseWebcamFunction() {
}

WebcamPrivateCloseWebcamFunction::~WebcamPrivateCloseWebcamFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateCloseWebcamFunction::Run() {
  std::unique_ptr<webcam_private::CloseWebcam::Params> params(
      webcam_private::CloseWebcam::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const bool success = WebcamPrivateAPI::Get(browser_context())
                           ->CloseWebcam(extension_id(), params->webcam_id);
  return RespondNow(success ? NoArguments() : Error(kUnknownErrorDoNotUse));
}

WebcamPrivateSetFunction::WebcamPrivateSetFunction() {
}

WebcamPrivateSetFunction::~WebcamPrivateSetFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateSetFunction::Run() {
  std::unique_ptr<webcam_private::Set::Params> params(
      webcam_private::Set::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Webcam* webcam = WebcamPrivateAPI::Get(browser_context())
                       ->GetWebcam(extension_id(), params->webcam_id);
  if (!webcam)
    return RespondNow(Error(kUnknownWebcam));

  int pan_speed = 0;
  int tilt_speed = 0;
  if (params->config.pan_speed)
    pan_speed = *(params->config.pan_speed);

  if (params->config.tilt_speed)
    tilt_speed = *(params->config.tilt_speed);

  pending_num_set_webcam_param_requests_ = 0;
  if (params->config.pan) {
    ++pending_num_set_webcam_param_requests_;
    webcam->SetPan(
        *(params->config.pan), pan_speed,
        base::Bind(&WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.pan_direction) {
    Webcam::PanDirection direction = Webcam::PAN_STOP;
    switch (params->config.pan_direction) {
      case webcam_private::PAN_DIRECTION_NONE:
      case webcam_private::PAN_DIRECTION_STOP:
        direction = Webcam::PAN_STOP;
        break;

      case webcam_private::PAN_DIRECTION_RIGHT:
        direction = Webcam::PAN_RIGHT;
        break;

      case webcam_private::PAN_DIRECTION_LEFT:
        direction = Webcam::PAN_LEFT;
        break;
    }
    ++pending_num_set_webcam_param_requests_;
    webcam->SetPanDirection(
        direction, pan_speed,
        base::Bind(&WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.tilt) {
    ++pending_num_set_webcam_param_requests_;
    webcam->SetTilt(
        *(params->config.tilt), tilt_speed,
        base::Bind(&WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.tilt_direction) {
    Webcam::TiltDirection direction = Webcam::TILT_STOP;
    switch (params->config.tilt_direction) {
      case webcam_private::TILT_DIRECTION_NONE:
      case webcam_private::TILT_DIRECTION_STOP:
        direction = Webcam::TILT_STOP;
        break;

      case webcam_private::TILT_DIRECTION_UP:
        direction = Webcam::TILT_UP;
        break;

      case webcam_private::TILT_DIRECTION_DOWN:
        direction = Webcam::TILT_DOWN;
        break;
    }
    ++pending_num_set_webcam_param_requests_;
    webcam->SetTiltDirection(
        direction, tilt_speed,
        base::Bind(&WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.zoom) {
    ++pending_num_set_webcam_param_requests_;
    webcam->SetZoom(
        *(params->config.zoom),
        base::Bind(&WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.autofocus_state) {
    Webcam::AutofocusState state = Webcam::AUTOFOCUS_ON;
    switch (params->config.autofocus_state) {
      case webcam_private::AUTOFOCUS_STATE_NONE:
      case webcam_private::AUTOFOCUS_STATE_OFF:
        state = Webcam::AUTOFOCUS_OFF;
        break;

      case webcam_private::AUTOFOCUS_STATE_ON:
        state = Webcam::AUTOFOCUS_ON;
        break;
    }
    ++pending_num_set_webcam_param_requests_;
    webcam->SetAutofocusState(
        state,
        base::Bind(&WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (params->config.focus) {
    ++pending_num_set_webcam_param_requests_;
    webcam->SetFocus(
        *(params->config.focus),
        base::Bind(&WebcamPrivateSetFunction::OnSetWebcamParameters, this));
  }

  if (pending_num_set_webcam_param_requests_ == 0)
    return AlreadyResponded();

  return RespondLater();
}

void WebcamPrivateSetFunction::OnSetWebcamParameters(bool success) {
  failed_ |= !success;
  --pending_num_set_webcam_param_requests_;

  DCHECK_GE(pending_num_set_webcam_param_requests_, 0);
  if (pending_num_set_webcam_param_requests_ == 0)
    Respond(failed_ ? Error(kSetWebcamPTZError) : NoArguments());
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
  std::unique_ptr<webcam_private::Get::Params> params(
      webcam_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Webcam* webcam = WebcamPrivateAPI::Get(browser_context())
                       ->GetWebcam(extension_id(), params->webcam_id);
  if (!webcam)
    return RespondNow(Error(kUnknownWebcam));

  webcam->GetPan(base::Bind(&WebcamPrivateGetFunction::OnGetWebcamParameters,
                            this, INQUIRY_PAN));
  webcam->GetTilt(base::Bind(&WebcamPrivateGetFunction::OnGetWebcamParameters,
                             this, INQUIRY_TILT));
  webcam->GetZoom(base::Bind(&WebcamPrivateGetFunction::OnGetWebcamParameters,
                             this, INQUIRY_ZOOM));
  webcam->GetFocus(base::Bind(&WebcamPrivateGetFunction::OnGetWebcamParameters,
                              this, INQUIRY_FOCUS));

  // We might have already responded through OnGetWebcamParameters().
  return did_respond() ? AlreadyResponded() : RespondLater();
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
      result.pan_range = std::make_unique<webcam_private::Range>();
      result.pan_range->min = min_pan_;
      result.pan_range->max = max_pan_;
    }
    if (min_tilt_ != max_tilt_) {
      result.tilt_range = std::make_unique<webcam_private::Range>();
      result.tilt_range->min = min_tilt_;
      result.tilt_range->max = max_tilt_;
    }
    if (min_zoom_ != max_zoom_) {
      result.zoom_range = std::make_unique<webcam_private::Range>();
      result.zoom_range->min = min_zoom_;
      result.zoom_range->max = max_zoom_;
    }
    if (min_focus_ != max_focus_) {
      result.focus_range = std::make_unique<webcam_private::Range>();
      result.focus_range->min = min_focus_;
      result.focus_range->max = max_focus_;
    }

    result.pan = pan_;
    result.tilt = tilt_;
    result.zoom = zoom_;
    result.focus = focus_;
    Respond(OneArgument(result.ToValue()));
  }
}

WebcamPrivateResetFunction::WebcamPrivateResetFunction() {
}

WebcamPrivateResetFunction::~WebcamPrivateResetFunction() {
}

ExtensionFunction::ResponseAction WebcamPrivateResetFunction::Run() {
  std::unique_ptr<webcam_private::Reset::Params> params(
      webcam_private::Reset::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Webcam* webcam = WebcamPrivateAPI::Get(browser_context())
                       ->GetWebcam(extension_id(), params->webcam_id);
  if (!webcam)
    return RespondNow(Error(kUnknownWebcam));

  webcam->Reset(params->config.pan != nullptr, params->config.tilt != nullptr,
                params->config.zoom != nullptr,
                base::Bind(&WebcamPrivateResetFunction::OnResetWebcam, this));

  // Reset() might have responded already.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void WebcamPrivateResetFunction::OnResetWebcam(bool success) {
  Respond(success ? NoArguments() : Error(kResetWebcamError));
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
