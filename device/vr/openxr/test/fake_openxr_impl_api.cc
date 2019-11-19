// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <directxmath.h>
#include <wrl.h>

#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/test/openxr_negotiate.h"
#include "device/vr/openxr/test/openxr_test_helper.h"

namespace {
// Global test helper that communicates with the test and contains the mock
// OpenXR runtime state/properties. A reference to this is returned as the
// instance handle through xrCreateInstance.
OpenXrTestHelper g_test_helper;
}  // namespace

// Mock implementations of openxr runtime.dll APIs.
// Please add new APIs in alphabetical order.

XrResult xrAcquireSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageAcquireInfo* acquire_info,
    uint32_t* index) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(acquire_info->type != XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrAcquireSwapchainImage type invalid");

  *index = g_test_helper.NextSwapchainImageIndex();

  return XR_SUCCESS;
}

XrResult xrAttachSessionActionSets(
    XrSession session,
    const XrSessionActionSetsAttachInfo* attach_info) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));

  return XR_SUCCESS;
}

XrResult xrBeginFrame(XrSession session,
                      const XrFrameBeginInfo* frame_begin_info) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(frame_begin_info->type != XR_TYPE_FRAME_BEGIN_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrFrameBeginInfo type invalid");

  return XR_SUCCESS;
}

XrResult xrBeginSession(XrSession session,
                        const XrSessionBeginInfo* begin_info) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(begin_info->type != XR_TYPE_SESSION_BEGIN_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSessionBeginInfo type invalid");
  RETURN_IF(begin_info->primaryViewConfigurationType !=
                OpenXrTestHelper::kViewConfigurationType,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSessionBeginInfo primaryViewConfigurationType invalid");

  RETURN_IF_XR_FAILED(g_test_helper.BeginSession());

  return XR_SUCCESS;
}

XrResult xrCreateAction(XrActionSet action_set,
                        const XrActionCreateInfo* create_info,
                        XrAction* action) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateActionSet(action_set));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateActionCreateInfo(*create_info));
  *action = g_test_helper.CreateAction(action_set, *create_info);

  return XR_SUCCESS;
}

XrResult xrCreateActionSet(XrInstance instance,
                           const XrActionSetCreateInfo* create_info,
                           XrActionSet* action_set) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateActionSetCreateInfo(*create_info));
  *action_set = g_test_helper.CreateActionSet(*create_info);

  return XR_SUCCESS;
}

XrResult xrCreateActionSpace(XrSession session,
                             const XrActionSpaceCreateInfo* create_info,
                             XrSpace* space) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidateActionSpaceCreateInfo(*create_info));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(create_info->action));
  *space = g_test_helper.CreateActionSpace(create_info->action);

  return XR_SUCCESS;
}

XrResult xrCreateInstance(const XrInstanceCreateInfo* create_info,
                          XrInstance* instance) {
  DVLOG(2) << __FUNCTION__;

  RETURN_IF(create_info->applicationInfo.apiVersion != XR_CURRENT_API_VERSION,
            XR_ERROR_API_VERSION_UNSUPPORTED, "apiVersion unsupported");

  RETURN_IF(create_info->type != XR_TYPE_INSTANCE_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrInstanceCreateInfo type invalid");

  RETURN_IF(create_info->enabledExtensionCount !=
                OpenXrTestHelper::NumExtensionsSupported(),
            XR_ERROR_VALIDATION_FAILURE, "enabledExtensionCount invalid");

  for (uint32_t i = 0; i < create_info->enabledExtensionCount; i++) {
    bool valid_extension = false;
    for (size_t j = 0; j < OpenXrTestHelper::NumExtensionsSupported(); j++) {
      if (strcmp(create_info->enabledExtensionNames[i],
                 OpenXrTestHelper::kExtensions[j]) == 0) {
        valid_extension = true;
        break;
      }
    }

    RETURN_IF_FALSE(valid_extension, XR_ERROR_VALIDATION_FAILURE,
                    "enabledExtensionNames contains invalid extensions");
  }

  *instance = g_test_helper.CreateInstance();

  return XR_SUCCESS;
}

XrResult xrCreateReferenceSpace(XrSession session,
                                const XrReferenceSpaceCreateInfo* create_info,
                                XrSpace* space) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(create_info->type != XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "XrReferenceSpaceCreateInfo type invalid");
  RETURN_IF(
      create_info->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_LOCAL &&
          create_info->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_VIEW &&
          create_info->referenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE,
      XR_ERROR_VALIDATION_FAILURE,
      "XrReferenceSpaceCreateInfo referenceSpaceType invalid");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateXrPosefIsIdentity(
      create_info->poseInReferenceSpace));

  *space = g_test_helper.CreateReferenceSpace(create_info->referenceSpaceType);

  return XR_SUCCESS;
}

XrResult xrCreateSession(XrInstance instance,
                         const XrSessionCreateInfo* create_info,
                         XrSession* session) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(create_info->type != XR_TYPE_SESSION_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSessionCreateInfo type invalid");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(create_info->systemId));

  const XrGraphicsBindingD3D11KHR* binding =
      static_cast<const XrGraphicsBindingD3D11KHR*>(create_info->next);
  RETURN_IF(binding->type != XR_TYPE_GRAPHICS_BINDING_D3D11_KHR,
            XR_ERROR_VALIDATION_FAILURE,
            "XrGraphicsBindingD3D11KHR type invalid");
  RETURN_IF(binding->device == nullptr, XR_ERROR_VALIDATION_FAILURE,
            "D3D11Device is null");

  g_test_helper.SetD3DDevice(binding->device);
  RETURN_IF_XR_FAILED(g_test_helper.GetSession(session));

  return XR_SUCCESS;
}

XrResult xrCreateSwapchain(XrSession session,
                           const XrSwapchainCreateInfo* create_info,
                           XrSwapchain* swapchain) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(create_info->type != XR_TYPE_SWAPCHAIN_CREATE_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSwapchainCreateInfo type invalid");
  RETURN_IF(create_info->arraySize != 1, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo arraySize invalid");
  RETURN_IF(create_info->format != DXGI_FORMAT_R8G8B8A8_UNORM,
            XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED,
            "XrSwapchainCreateInfo format unsupported");
  RETURN_IF(create_info->width != OpenXrTestHelper::kDimension * 2,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo width is not dimension * 2");
  RETURN_IF(create_info->height != OpenXrTestHelper::kDimension,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo height is not dimension");
  RETURN_IF(create_info->mipCount != 1, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo mipCount is not 1");
  RETURN_IF(create_info->faceCount != 1, XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo faceCount is not 1");
  RETURN_IF(create_info->sampleCount != OpenXrTestHelper::kSwapCount,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo sampleCount invalid");
  RETURN_IF(create_info->usageFlags != XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
            XR_ERROR_VALIDATION_FAILURE,
            "XrSwapchainCreateInfo usageFlags is not "
            "XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT");

  *swapchain = g_test_helper.GetSwapchain();

  return XR_SUCCESS;
}

XrResult xrDestroyActionSet(XrActionSet action_set) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateActionSet(action_set));
  return XR_SUCCESS;
}

XrResult xrDestroyInstance(XrInstance instance) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  g_test_helper.Reset();
  return XR_SUCCESS;
}

XrResult xrDestroySpace(XrSpace space) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(space));

  return XR_SUCCESS;
}

XrResult xrEndFrame(XrSession session, const XrFrameEndInfo* frame_end_info) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(frame_end_info->type != XR_TYPE_FRAME_END_INFO,
            XR_ERROR_VALIDATION_FAILURE, "frame_end_info type invalid");
  RETURN_IF(frame_end_info->environmentBlendMode !=
                OpenXrTestHelper::kEnvironmentBlendMode,
            XR_ERROR_VALIDATION_FAILURE,
            "frame_end_info environmentBlendMode invalid");
  RETURN_IF(frame_end_info->layerCount != 1, XR_ERROR_VALIDATION_FAILURE,
            "frame_end_info layerCount invalid");
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidatePredictedDisplayTime(frame_end_info->displayTime));

  for (uint32_t i = 0; i < frame_end_info->layerCount; i++) {
    const XrCompositionLayerProjection* multi_projection_layer_ptr =
        reinterpret_cast<const XrCompositionLayerProjection*>(
            frame_end_info->layers[i]);
    RETURN_IF(multi_projection_layer_ptr->type !=
                  XR_TYPE_COMPOSITION_LAYER_PROJECTION,
              XR_ERROR_VALIDATION_FAILURE,
              "XrCompositionLayerProjection type invalid");

    RETURN_IF(
        multi_projection_layer_ptr->viewCount != OpenXrTestHelper::kViewCount,
        XR_ERROR_VALIDATION_FAILURE,
        "XrCompositionLayerProjection viewCount invalid");
    for (uint32_t j = 0; j < multi_projection_layer_ptr->viewCount; j++) {
      RETURN_IF(multi_projection_layer_ptr->views[j].type !=
                    XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
                XR_ERROR_VALIDATION_FAILURE,
                "XrCompositionLayerProjectionView type invalid");
    }
  }
  g_test_helper.OnPresentedFrame();

  return XR_SUCCESS;
}

XrResult xrEndSession(XrSession session) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_XR_FAILED(g_test_helper.EndSession());

  return XR_SUCCESS;
}

XrResult xrEnumerateEnvironmentBlendModes(
    XrInstance instance,
    XrSystemId system_id,
    XrViewConfigurationType view_configuration_type,
    uint32_t environmentBlendModeCapacityInput,
    uint32_t* environmentBlendModeCountOutput,
    XrEnvironmentBlendMode* environmentBlendModes) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF(view_configuration_type != OpenXrTestHelper::kViewConfigurationType,
            XR_ERROR_VALIDATION_FAILURE,
            "xrEnumerateEnvironmentBlendModes viewConfigurationType invalid");

  *environmentBlendModeCountOutput = 1;

  if (environmentBlendModeCapacityInput != 0) {
    *environmentBlendModes = OpenXrTestHelper::kEnvironmentBlendMode;
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateInstanceExtensionProperties(
    const char* layer_name,
    uint32_t property_capacity_input,
    uint32_t* property_count_output,
    XrExtensionProperties* properties) {
  DVLOG(2) << __FUNCTION__;

  RETURN_IF(
      property_capacity_input < OpenXrTestHelper::NumExtensionsSupported() &&
          property_capacity_input != 0,
      XR_ERROR_SIZE_INSUFFICIENT, "XrExtensionProperties array is too small");

  *property_count_output = OpenXrTestHelper::NumExtensionsSupported();

  if (property_capacity_input != 0) {
    for (uint32_t i = 0; i < OpenXrTestHelper::NumExtensionsSupported(); i++) {
      errno_t error = strcpy_s(properties[i].extensionName,
                               OpenXrTestHelper::kExtensions[i]);
      DCHECK(error == 0);
      properties[i].extensionVersion = 1;
    }
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateViewConfigurationViews(
    XrInstance instance,
    XrSystemId system_id,
    XrViewConfigurationType view_configuration_type,
    uint32_t view_capacity_input,
    uint32_t* view_count_output,
    XrViewConfigurationView* views) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF(view_configuration_type != OpenXrTestHelper::kViewConfigurationType,
            XR_ERROR_VALIDATION_FAILURE,
            "xrEnumerateViewConfigurationViews viewConfigurationType invalid");

  *view_count_output = OpenXrTestHelper::NumViews();

  if (view_capacity_input != 0) {
    views[0] = OpenXrTestHelper::kViewConfigurationViews[0];
    views[1] = OpenXrTestHelper::kViewConfigurationViews[1];
  }

  return XR_SUCCESS;
}

XrResult xrEnumerateSwapchainImages(XrSwapchain swapchain,
                                    uint32_t image_capacity_input,
                                    uint32_t* image_count_output,
                                    XrSwapchainImageBaseHeader* images) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(image_capacity_input != OpenXrTestHelper::kMinSwapchainBuffering &&
                image_capacity_input != 0,
            XR_ERROR_SIZE_INSUFFICIENT,
            "xrEnumerateSwapchainImages does not equal length returned by "
            "xrCreateSwapchain");

  *image_count_output = OpenXrTestHelper::kMinSwapchainBuffering;

  if (image_capacity_input != 0) {
    const std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>>& textures =
        g_test_helper.GetSwapchainTextures();
    DCHECK(textures.size() == image_capacity_input);

    for (uint32_t i = 0; i < image_capacity_input; i++) {
      XrSwapchainImageD3D11KHR& image =
          reinterpret_cast<XrSwapchainImageD3D11KHR*>(images)[i];

      RETURN_IF(image.type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
                XR_ERROR_VALIDATION_FAILURE,
                "XrSwapchainImageD3D11KHR type invalid");

      image.texture = textures[i].Get();
    }
  }

  return XR_SUCCESS;
}

XrResult xrGetD3D11GraphicsRequirementsKHR(
    XrInstance instance,
    XrSystemId system_id,
    XrGraphicsRequirementsD3D11KHR* graphics_requirements) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSystemId(system_id));
  RETURN_IF(
      graphics_requirements->type != XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR,
      XR_ERROR_VALIDATION_FAILURE,
      "XrGraphicsRequirementsD3D11KHR type invalid");

  Microsoft::WRL::ComPtr<IDXGIFactory1> dxgi_factory;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
  DCHECK(SUCCEEDED(hr));
  for (int i = 0; SUCCEEDED(dxgi_factory->EnumAdapters(i, &adapter)); i++) {
    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
    graphics_requirements->adapterLuid = desc.AdapterLuid;

    // Require D3D11.1 to support shared NT handles.
    graphics_requirements->minFeatureLevel = D3D_FEATURE_LEVEL_11_1;

    return XR_SUCCESS;
  }

  RETURN_IF_FALSE(false, XR_ERROR_VALIDATION_FAILURE,
                  "Unable to create query DXGI Adapter");
}

XrResult xrGetActionStateFloat(XrSession session,
                               const XrActionStateGetInfo* get_info,
                               XrActionStateFloat* state) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateFloat get_info has wrong type");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(get_info->subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateFloat has subactionPath != nullptr which is not "
            "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStateFloat(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetActionStateBoolean(XrSession session,
                                 const XrActionStateGetInfo* get_info,
                                 XrActionStateBoolean* state) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateBoolean get_info has wrong type");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(get_info->subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateBoolean has subactionPath != nullptr which is not "
            "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStateBoolean(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetActionStateVector2f(XrSession session,
                                  const XrActionStateGetInfo* get_info,
                                  XrActionStateVector2f* state) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStateVector2f get_info has wrong type");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(
      get_info->subactionPath != XR_NULL_PATH, XR_ERROR_VALIDATION_FAILURE,
      "xrGetActionStateVector2f has subactionPath != nullptr which is not "
      "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStateVector2f(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetActionStatePose(XrSession session,
                              const XrActionStateGetInfo* get_info,
                              XrActionStatePose* state) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(get_info->type != XR_TYPE_ACTION_STATE_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStatePose get_info has wrong type");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateAction(get_info->action));
  RETURN_IF(get_info->subactionPath != XR_NULL_PATH,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetActionStatePose has subactionPath != nullptr which is not "
            "supported by current version of test.");
  RETURN_IF_XR_FAILED(
      g_test_helper.GetActionStatePose(get_info->action, state));

  return XR_SUCCESS;
}

XrResult xrGetInstanceProperties(XrInstance instance,
                                 XrInstanceProperties* instanceProperties) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(instanceProperties->type != XR_TYPE_INSTANCE_PROPERTIES,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetReferenceSpaceBoundsRect type is not stage");
  errno_t error =
      strcpy_s(instanceProperties->runtimeName, "OpenXR Mock Runtime");
  DCHECK(error == 0);
  return XR_SUCCESS;
}

XrResult xrGetCurrentInteractionProfile(
    XrSession session,
    XrPath topLevelUserPath,
    XrInteractionProfileState* interactionProfile) {
  DVLOG(1) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(interactionProfile->type != XR_TYPE_INTERACTION_PROFILE_STATE,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetCurrentInteractionProfile type is not "
            "XR_TYPE_INTERACTION_PROFILE_STATE");
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePath(topLevelUserPath));
  interactionProfile->interactionProfile =
      g_test_helper.GetCurrentInteractionProfile();
  return XR_SUCCESS;
}

XrResult xrGetReferenceSpaceBoundsRect(XrSession session,
                                       XrReferenceSpaceType referenceSpaceType,
                                       XrExtent2Df* bounds) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(referenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE,
            XR_ERROR_VALIDATION_FAILURE,
            "xrGetReferenceSpaceBoundsRect type is not stage");
  bounds->width = 0;
  bounds->height = 0;
  return XR_SUCCESS;
}

XrResult xrGetSystem(XrInstance instance,
                     const XrSystemGetInfo* get_info,
                     XrSystemId* system_id) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(get_info->type != XR_TYPE_SYSTEM_GET_INFO,
            XR_ERROR_VALIDATION_FAILURE, "XrSystemGetInfo type invalid");
  RETURN_IF(get_info->formFactor != XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
            XR_ERROR_VALIDATION_FAILURE, "XrSystemGetInfo formFactor invalid");

  *system_id = g_test_helper.GetSystemId();

  return XR_SUCCESS;
}

XrResult xrGetSystemProperties(XrInstance instance,
                               XrSystemId systemId,
                               XrSystemProperties* properties) {
  properties->trackingProperties.positionTracking = true;
  return XR_SUCCESS;
}

XrResult xrLocateSpace(XrSpace space,
                       XrSpace baseSpace,
                       XrTime time,
                       XrSpaceLocation* location) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(space));
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(baseSpace));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePredictedDisplayTime(time));

  g_test_helper.LocateSpace(space, &(location->pose));

  location->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                            XR_SPACE_LOCATION_POSITION_VALID_BIT;

  return XR_SUCCESS;
}

XrResult xrLocateViews(XrSession session,
                       const XrViewLocateInfo* view_locate_info,
                       XrViewState* view_state,
                       uint32_t view_capacity_input,
                       uint32_t* view_count_output,
                       XrView* views) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePredictedDisplayTime(
      view_locate_info->displayTime));
  RETURN_IF(view_locate_info->type != XR_TYPE_VIEW_LOCATE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrLocateViews view_locate_info type invalid");
  RETURN_IF_XR_FAILED(g_test_helper.ValidateSpace(view_locate_info->space));
  if (view_capacity_input != 0) {
    RETURN_IF_XR_FAILED(
        g_test_helper.ValidateViews(view_capacity_input, views));
    RETURN_IF_FALSE(g_test_helper.UpdateViewFOV(views, view_capacity_input),
                    XR_ERROR_VALIDATION_FAILURE,
                    "xrLocateViews UpdateViewFOV failed");
    *view_count_output = OpenXrTestHelper::kViewCount;
    view_state->viewStateFlags =
        XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
  }

  return XR_SUCCESS;
}

XrResult xrPollEvent(XrInstance instance, XrEventDataBuffer* event_data) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));

  RETURN_IF_FALSE(event_data->type == XR_TYPE_EVENT_DATA_BUFFER,
                  XR_ERROR_VALIDATION_FAILURE,
                  "xrPollEvent event_data type invalid");

  return g_test_helper.PollEvent(event_data);
}

XrResult xrReleaseSwapchainImage(
    XrSwapchain swapchain,
    const XrSwapchainImageReleaseInfo* release_info) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(release_info->type != XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
            XR_ERROR_VALIDATION_FAILURE,
            "xrReleaseSwapchainImage type invalid");

  return XR_SUCCESS;
}

XrResult xrSuggestInteractionProfileBindings(
    XrInstance instance,
    const XrInteractionProfileSuggestedBinding* suggested_bindings) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF(
      suggested_bindings->type != XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
      XR_ERROR_VALIDATION_FAILURE,
      "xrSetInteractionProfileSuggestedBindings type invalid");
  RETURN_IF_XR_FAILED(
      g_test_helper.ValidatePath(suggested_bindings->interactionProfile));
  RETURN_IF(
      g_test_helper.PathToString(suggested_bindings->interactionProfile)
              .compare("/interaction_profiles/microsoft/motion_controller") !=
          0,
      XR_ERROR_VALIDATION_FAILURE,
      "xrSetInteractionProfileSuggestedBindings interactionProfile other than "
      "microsoft is used, this is not currently supported.");

  for (uint32_t i = 0; i < suggested_bindings->countSuggestedBindings; i++) {
    XrActionSuggestedBinding suggestedBinding =
        suggested_bindings->suggestedBindings[i];
    RETURN_IF_XR_FAILED(g_test_helper.BindActionAndPath(suggestedBinding));
  }

  return XR_SUCCESS;
}

XrResult xrStringToPath(XrInstance instance,
                        const char* pathString,
                        XrPath* path) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));

  *path = g_test_helper.GetPath(pathString);

  return XR_SUCCESS;
}

XrResult xrPathToString(XrInstance instance,
                        XrPath path,
                        uint32_t bufferCapacityInput,
                        uint32_t* bufferCountOutput,
                        char* buffer) {
  DVLOG(1) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateInstance(instance));
  RETURN_IF_XR_FAILED(g_test_helper.ValidatePath(path));

  std::string path_string = g_test_helper.PathToString(path);

  if (bufferCapacityInput == 0) {
    // OpenXR spec counts terminating '\0'
    *bufferCountOutput = path_string.size() + 1;
    return XR_SUCCESS;
  }

  RETURN_IF(
      *bufferCountOutput <= path_string.size(), XR_ERROR_SIZE_INSUFFICIENT,
      "xrPathToString inputsize is not large enough to hold the output string");
  errno_t error = strcpy_s(buffer, *bufferCountOutput, path_string.data());
  DCHECK(error == 0);

  return XR_SUCCESS;
}

XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* sync_info) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF_FALSE(g_test_helper.UpdateData(), XR_ERROR_VALIDATION_FAILURE,
                  "xrSyncActionData can't receive data from test");

  for (uint32_t i = 0; i < sync_info->countActiveActionSets; i++) {
    RETURN_IF(
        sync_info->activeActionSets[i].subactionPath != XR_NULL_PATH,
        XR_ERROR_VALIDATION_FAILURE,
        "xrSyncActionData does not support use of subactionPath for test yet");
    RETURN_IF_XR_FAILED(
        g_test_helper.SyncActionData(sync_info->activeActionSets[i].actionSet));
  }

  return XR_SUCCESS;
}

XrResult xrWaitFrame(XrSession session,
                     const XrFrameWaitInfo* frame_wait_info,
                     XrFrameState* frame_state) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSession(session));
  RETURN_IF(frame_wait_info->type != XR_TYPE_FRAME_WAIT_INFO,
            XR_ERROR_VALIDATION_FAILURE, "frame_wait_info type invalid");
  RETURN_IF(frame_state->type != XR_TYPE_FRAME_STATE,
            XR_ERROR_VALIDATION_FAILURE, "XR_TYPE_FRAME_STATE type invalid");

  frame_state->predictedDisplayTime = g_test_helper.NextPredictedDisplayTime();

  return XR_SUCCESS;
}

XrResult xrWaitSwapchainImage(XrSwapchain swapchain,
                              const XrSwapchainImageWaitInfo* wait_info) {
  DVLOG(2) << __FUNCTION__;
  XrResult xr_result;

  RETURN_IF_XR_FAILED(g_test_helper.ValidateSwapchain(swapchain));
  RETURN_IF(wait_info->type != XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
            XR_ERROR_VALIDATION_FAILURE, "xrWaitSwapchainImage type invalid");
  RETURN_IF(wait_info->timeout != XR_INFINITE_DURATION,
            XR_ERROR_VALIDATION_FAILURE,
            "xrWaitSwapchainImage timeout not XR_INFINITE_DURATION");

  return XR_SUCCESS;
}
