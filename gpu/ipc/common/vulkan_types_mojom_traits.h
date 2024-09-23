// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/ipc/common/generate_vulkan_types.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_

#include <string_view>

#include "base/containers/span.h"
#include "gpu/ipc/common/vulkan_types.h"
#include "gpu/ipc/common/vulkan_types.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::VkExtensionPropertiesDataView,
                    VkExtensionProperties> {
  static std::string_view extensionName(const VkExtensionProperties& input) {
    return input.extensionName;
  }

  static uint32_t specVersion(const VkExtensionProperties& input) {
    return input.specVersion;
  }

  static bool Read(gpu::mojom::VkExtensionPropertiesDataView data,
                   VkExtensionProperties* out);
};

template <>
struct StructTraits<gpu::mojom::VkLayerPropertiesDataView, VkLayerProperties> {
  static std::string_view layerName(const VkLayerProperties& input) {
    return input.layerName;
  }

  static uint32_t specVersion(const VkLayerProperties& input) {
    return input.specVersion;
  }

  static uint32_t implementationVersion(const VkLayerProperties& input) {
    return input.implementationVersion;
  }

  static std::string_view description(const VkLayerProperties& input) {
    return input.description;
  }

  static bool Read(gpu::mojom::VkLayerPropertiesDataView data,
                   VkLayerProperties* out);
};

template <>
struct StructTraits<gpu::mojom::VkPhysicalDevicePropertiesDataView,
                    VkPhysicalDeviceProperties> {
  static uint32_t apiVersion(const VkPhysicalDeviceProperties& input) {
    return input.apiVersion;
  }

  static uint32_t driverVersion(const VkPhysicalDeviceProperties& input) {
    return input.driverVersion;
  }

  static uint32_t vendorID(const VkPhysicalDeviceProperties& input) {
    return input.vendorID;
  }

  static uint32_t deviceID(const VkPhysicalDeviceProperties& input) {
    return input.deviceID;
  }

  static VkPhysicalDeviceType deviceType(
      const VkPhysicalDeviceProperties& input) {
    return input.deviceType;
  }

  static std::string_view deviceName(const VkPhysicalDeviceProperties& input) {
    return input.deviceName;
  }

  static base::span<const uint8_t> pipelineCacheUUID(
      const VkPhysicalDeviceProperties& input) {
    return input.pipelineCacheUUID;
  }

  static const VkPhysicalDeviceLimits& limits(
      const VkPhysicalDeviceProperties& input) {
    return input.limits;
  }

  static const VkPhysicalDeviceSparseProperties& sparseProperties(
      const VkPhysicalDeviceProperties& input) {
    return input.sparseProperties;
  }

  static bool Read(gpu::mojom::VkPhysicalDevicePropertiesDataView data,
                   VkPhysicalDeviceProperties* out);
};

template <>
struct EnumTraits<gpu::mojom::VkPhysicalDeviceType, VkPhysicalDeviceType> {
  static gpu::mojom::VkPhysicalDeviceType ToMojom(VkPhysicalDeviceType input) {
    switch (input) {
      case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return gpu::mojom::VkPhysicalDeviceType::OTHER;
      case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return gpu::mojom::VkPhysicalDeviceType::INTEGRATED_GPU;
      case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return gpu::mojom::VkPhysicalDeviceType::DISCRETE_GPU;
      case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return gpu::mojom::VkPhysicalDeviceType::VIRTUAL_GPU;
      case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU:
        return gpu::mojom::VkPhysicalDeviceType::CPU;
      default:
        NOTREACHED_IN_MIGRATION();
        return gpu::mojom::VkPhysicalDeviceType::INVALID_VALUE;
    }
  }

  static bool FromMojom(gpu::mojom::VkPhysicalDeviceType input,
                        VkPhysicalDeviceType* out) {
    switch (input) {
      case gpu::mojom::VkPhysicalDeviceType::OTHER:
        *out = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER;
        return true;
      case gpu::mojom::VkPhysicalDeviceType::INTEGRATED_GPU:
        *out = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        return true;
      case gpu::mojom::VkPhysicalDeviceType::DISCRETE_GPU:
        *out = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        return true;
      case gpu::mojom::VkPhysicalDeviceType::VIRTUAL_GPU:
        *out = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU;
        return true;
      case gpu::mojom::VkPhysicalDeviceType::CPU:
        *out = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU;
        return true;
      case gpu::mojom::VkPhysicalDeviceType::INVALID_VALUE:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};
template <>
struct StructTraits<gpu::mojom::VkPhysicalDeviceLimitsDataView,
                    VkPhysicalDeviceLimits> {
  static uint32_t maxImageDimension1D(const VkPhysicalDeviceLimits& input) {
    return input.maxImageDimension1D;
  }

  static uint32_t maxImageDimension2D(const VkPhysicalDeviceLimits& input) {
    return input.maxImageDimension2D;
  }

  static uint32_t maxImageDimension3D(const VkPhysicalDeviceLimits& input) {
    return input.maxImageDimension3D;
  }

  static uint32_t maxImageDimensionCube(const VkPhysicalDeviceLimits& input) {
    return input.maxImageDimensionCube;
  }

  static uint32_t maxImageArrayLayers(const VkPhysicalDeviceLimits& input) {
    return input.maxImageArrayLayers;
  }

  static uint32_t maxTexelBufferElements(const VkPhysicalDeviceLimits& input) {
    return input.maxTexelBufferElements;
  }

  static uint32_t maxUniformBufferRange(const VkPhysicalDeviceLimits& input) {
    return input.maxUniformBufferRange;
  }

  static uint32_t maxStorageBufferRange(const VkPhysicalDeviceLimits& input) {
    return input.maxStorageBufferRange;
  }

  static uint32_t maxPushConstantsSize(const VkPhysicalDeviceLimits& input) {
    return input.maxPushConstantsSize;
  }

  static uint32_t maxMemoryAllocationCount(
      const VkPhysicalDeviceLimits& input) {
    return input.maxMemoryAllocationCount;
  }

  static uint32_t maxSamplerAllocationCount(
      const VkPhysicalDeviceLimits& input) {
    return input.maxSamplerAllocationCount;
  }

  static bool bufferImageGranularity(const VkPhysicalDeviceLimits& input) {
    return input.bufferImageGranularity;
  }

  static bool sparseAddressSpaceSize(const VkPhysicalDeviceLimits& input) {
    return input.sparseAddressSpaceSize;
  }

  static uint32_t maxBoundDescriptorSets(const VkPhysicalDeviceLimits& input) {
    return input.maxBoundDescriptorSets;
  }

  static uint32_t maxPerStageDescriptorSamplers(
      const VkPhysicalDeviceLimits& input) {
    return input.maxPerStageDescriptorSamplers;
  }

  static uint32_t maxPerStageDescriptorUniformBuffers(
      const VkPhysicalDeviceLimits& input) {
    return input.maxPerStageDescriptorUniformBuffers;
  }

  static uint32_t maxPerStageDescriptorStorageBuffers(
      const VkPhysicalDeviceLimits& input) {
    return input.maxPerStageDescriptorStorageBuffers;
  }

  static uint32_t maxPerStageDescriptorSampledImages(
      const VkPhysicalDeviceLimits& input) {
    return input.maxPerStageDescriptorSampledImages;
  }

  static uint32_t maxPerStageDescriptorStorageImages(
      const VkPhysicalDeviceLimits& input) {
    return input.maxPerStageDescriptorStorageImages;
  }

  static uint32_t maxPerStageDescriptorInputAttachments(
      const VkPhysicalDeviceLimits& input) {
    return input.maxPerStageDescriptorInputAttachments;
  }

  static uint32_t maxPerStageResources(const VkPhysicalDeviceLimits& input) {
    return input.maxPerStageResources;
  }

  static uint32_t maxDescriptorSetSamplers(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetSamplers;
  }

  static uint32_t maxDescriptorSetUniformBuffers(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetUniformBuffers;
  }

  static uint32_t maxDescriptorSetUniformBuffersDynamic(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetUniformBuffersDynamic;
  }

  static uint32_t maxDescriptorSetStorageBuffers(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetStorageBuffers;
  }

  static uint32_t maxDescriptorSetStorageBuffersDynamic(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetStorageBuffersDynamic;
  }

  static uint32_t maxDescriptorSetSampledImages(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetSampledImages;
  }

  static uint32_t maxDescriptorSetStorageImages(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetStorageImages;
  }

  static uint32_t maxDescriptorSetInputAttachments(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDescriptorSetInputAttachments;
  }

  static uint32_t maxVertexInputAttributes(
      const VkPhysicalDeviceLimits& input) {
    return input.maxVertexInputAttributes;
  }

  static uint32_t maxVertexInputBindings(const VkPhysicalDeviceLimits& input) {
    return input.maxVertexInputBindings;
  }

  static uint32_t maxVertexInputAttributeOffset(
      const VkPhysicalDeviceLimits& input) {
    return input.maxVertexInputAttributeOffset;
  }

  static uint32_t maxVertexInputBindingStride(
      const VkPhysicalDeviceLimits& input) {
    return input.maxVertexInputBindingStride;
  }

  static uint32_t maxVertexOutputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxVertexOutputComponents;
  }

  static uint32_t maxTessellationGenerationLevel(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationGenerationLevel;
  }

  static uint32_t maxTessellationPatchSize(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationPatchSize;
  }

  static uint32_t maxTessellationControlPerVertexInputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationControlPerVertexInputComponents;
  }

  static uint32_t maxTessellationControlPerVertexOutputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationControlPerVertexOutputComponents;
  }

  static uint32_t maxTessellationControlPerPatchOutputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationControlPerPatchOutputComponents;
  }

  static uint32_t maxTessellationControlTotalOutputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationControlTotalOutputComponents;
  }

  static uint32_t maxTessellationEvaluationInputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationEvaluationInputComponents;
  }

  static uint32_t maxTessellationEvaluationOutputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxTessellationEvaluationOutputComponents;
  }

  static uint32_t maxGeometryShaderInvocations(
      const VkPhysicalDeviceLimits& input) {
    return input.maxGeometryShaderInvocations;
  }

  static uint32_t maxGeometryInputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxGeometryInputComponents;
  }

  static uint32_t maxGeometryOutputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxGeometryOutputComponents;
  }

  static uint32_t maxGeometryOutputVertices(
      const VkPhysicalDeviceLimits& input) {
    return input.maxGeometryOutputVertices;
  }

  static uint32_t maxGeometryTotalOutputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxGeometryTotalOutputComponents;
  }

  static uint32_t maxFragmentInputComponents(
      const VkPhysicalDeviceLimits& input) {
    return input.maxFragmentInputComponents;
  }

  static uint32_t maxFragmentOutputAttachments(
      const VkPhysicalDeviceLimits& input) {
    return input.maxFragmentOutputAttachments;
  }

  static uint32_t maxFragmentDualSrcAttachments(
      const VkPhysicalDeviceLimits& input) {
    return input.maxFragmentDualSrcAttachments;
  }

  static uint32_t maxFragmentCombinedOutputResources(
      const VkPhysicalDeviceLimits& input) {
    return input.maxFragmentCombinedOutputResources;
  }

  static uint32_t maxComputeSharedMemorySize(
      const VkPhysicalDeviceLimits& input) {
    return input.maxComputeSharedMemorySize;
  }

  static base::span<const uint32_t> maxComputeWorkGroupCount(
      const VkPhysicalDeviceLimits& input) {
    return input.maxComputeWorkGroupCount;
  }

  static uint32_t maxComputeWorkGroupInvocations(
      const VkPhysicalDeviceLimits& input) {
    return input.maxComputeWorkGroupInvocations;
  }

  static base::span<const uint32_t> maxComputeWorkGroupSize(
      const VkPhysicalDeviceLimits& input) {
    return input.maxComputeWorkGroupSize;
  }

  static uint32_t subPixelPrecisionBits(const VkPhysicalDeviceLimits& input) {
    return input.subPixelPrecisionBits;
  }

  static uint32_t subTexelPrecisionBits(const VkPhysicalDeviceLimits& input) {
    return input.subTexelPrecisionBits;
  }

  static uint32_t mipmapPrecisionBits(const VkPhysicalDeviceLimits& input) {
    return input.mipmapPrecisionBits;
  }

  static uint32_t maxDrawIndexedIndexValue(
      const VkPhysicalDeviceLimits& input) {
    return input.maxDrawIndexedIndexValue;
  }

  static uint32_t maxDrawIndirectCount(const VkPhysicalDeviceLimits& input) {
    return input.maxDrawIndirectCount;
  }

  static float maxSamplerLodBias(const VkPhysicalDeviceLimits& input) {
    return input.maxSamplerLodBias;
  }

  static float maxSamplerAnisotropy(const VkPhysicalDeviceLimits& input) {
    return input.maxSamplerAnisotropy;
  }

  static uint32_t maxViewports(const VkPhysicalDeviceLimits& input) {
    return input.maxViewports;
  }

  static base::span<const uint32_t> maxViewportDimensions(
      const VkPhysicalDeviceLimits& input) {
    return input.maxViewportDimensions;
  }

  static base::span<const float> viewportBoundsRange(
      const VkPhysicalDeviceLimits& input) {
    return input.viewportBoundsRange;
  }

  static uint32_t viewportSubPixelBits(const VkPhysicalDeviceLimits& input) {
    return input.viewportSubPixelBits;
  }

  static size_t minMemoryMapAlignment(const VkPhysicalDeviceLimits& input) {
    return input.minMemoryMapAlignment;
  }

  static bool minTexelBufferOffsetAlignment(
      const VkPhysicalDeviceLimits& input) {
    return input.minTexelBufferOffsetAlignment;
  }

  static bool minUniformBufferOffsetAlignment(
      const VkPhysicalDeviceLimits& input) {
    return input.minUniformBufferOffsetAlignment;
  }

  static bool minStorageBufferOffsetAlignment(
      const VkPhysicalDeviceLimits& input) {
    return input.minStorageBufferOffsetAlignment;
  }

  static int32_t minTexelOffset(const VkPhysicalDeviceLimits& input) {
    return input.minTexelOffset;
  }

  static uint32_t maxTexelOffset(const VkPhysicalDeviceLimits& input) {
    return input.maxTexelOffset;
  }

  static int32_t minTexelGatherOffset(const VkPhysicalDeviceLimits& input) {
    return input.minTexelGatherOffset;
  }

  static uint32_t maxTexelGatherOffset(const VkPhysicalDeviceLimits& input) {
    return input.maxTexelGatherOffset;
  }

  static float minInterpolationOffset(const VkPhysicalDeviceLimits& input) {
    return input.minInterpolationOffset;
  }

  static float maxInterpolationOffset(const VkPhysicalDeviceLimits& input) {
    return input.maxInterpolationOffset;
  }

  static uint32_t subPixelInterpolationOffsetBits(
      const VkPhysicalDeviceLimits& input) {
    return input.subPixelInterpolationOffsetBits;
  }

  static uint32_t maxFramebufferWidth(const VkPhysicalDeviceLimits& input) {
    return input.maxFramebufferWidth;
  }

  static uint32_t maxFramebufferHeight(const VkPhysicalDeviceLimits& input) {
    return input.maxFramebufferHeight;
  }

  static uint32_t maxFramebufferLayers(const VkPhysicalDeviceLimits& input) {
    return input.maxFramebufferLayers;
  }

  static VkSampleCountFlags framebufferColorSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.framebufferColorSampleCounts;
  }

  static VkSampleCountFlags framebufferDepthSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.framebufferDepthSampleCounts;
  }

  static VkSampleCountFlags framebufferStencilSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.framebufferStencilSampleCounts;
  }

  static VkSampleCountFlags framebufferNoAttachmentsSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.framebufferNoAttachmentsSampleCounts;
  }

  static uint32_t maxColorAttachments(const VkPhysicalDeviceLimits& input) {
    return input.maxColorAttachments;
  }

  static VkSampleCountFlags sampledImageColorSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.sampledImageColorSampleCounts;
  }

  static VkSampleCountFlags sampledImageIntegerSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.sampledImageIntegerSampleCounts;
  }

  static VkSampleCountFlags sampledImageDepthSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.sampledImageDepthSampleCounts;
  }

  static VkSampleCountFlags sampledImageStencilSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.sampledImageStencilSampleCounts;
  }

  static VkSampleCountFlags storageImageSampleCounts(
      const VkPhysicalDeviceLimits& input) {
    return input.storageImageSampleCounts;
  }

  static uint32_t maxSampleMaskWords(const VkPhysicalDeviceLimits& input) {
    return input.maxSampleMaskWords;
  }

  static bool timestampComputeAndGraphics(const VkPhysicalDeviceLimits& input) {
    return input.timestampComputeAndGraphics;
  }

  static float timestampPeriod(const VkPhysicalDeviceLimits& input) {
    return input.timestampPeriod;
  }

  static uint32_t maxClipDistances(const VkPhysicalDeviceLimits& input) {
    return input.maxClipDistances;
  }

  static uint32_t maxCullDistances(const VkPhysicalDeviceLimits& input) {
    return input.maxCullDistances;
  }

  static uint32_t maxCombinedClipAndCullDistances(
      const VkPhysicalDeviceLimits& input) {
    return input.maxCombinedClipAndCullDistances;
  }

  static uint32_t discreteQueuePriorities(const VkPhysicalDeviceLimits& input) {
    return input.discreteQueuePriorities;
  }

  static base::span<const float> pointSizeRange(
      const VkPhysicalDeviceLimits& input) {
    return input.pointSizeRange;
  }

  static base::span<const float> lineWidthRange(
      const VkPhysicalDeviceLimits& input) {
    return input.lineWidthRange;
  }

  static float pointSizeGranularity(const VkPhysicalDeviceLimits& input) {
    return input.pointSizeGranularity;
  }

  static float lineWidthGranularity(const VkPhysicalDeviceLimits& input) {
    return input.lineWidthGranularity;
  }

  static bool strictLines(const VkPhysicalDeviceLimits& input) {
    return input.strictLines;
  }

  static bool standardSampleLocations(const VkPhysicalDeviceLimits& input) {
    return input.standardSampleLocations;
  }

  static bool optimalBufferCopyOffsetAlignment(
      const VkPhysicalDeviceLimits& input) {
    return input.optimalBufferCopyOffsetAlignment;
  }

  static bool optimalBufferCopyRowPitchAlignment(
      const VkPhysicalDeviceLimits& input) {
    return input.optimalBufferCopyRowPitchAlignment;
  }

  static bool nonCoherentAtomSize(const VkPhysicalDeviceLimits& input) {
    return input.nonCoherentAtomSize;
  }

  static bool Read(gpu::mojom::VkPhysicalDeviceLimitsDataView data,
                   VkPhysicalDeviceLimits* out);
};

template <>
struct StructTraits<gpu::mojom::VkPhysicalDeviceSparsePropertiesDataView,
                    VkPhysicalDeviceSparseProperties> {
  static bool residencyStandard2DBlockShape(
      const VkPhysicalDeviceSparseProperties& input) {
    return input.residencyStandard2DBlockShape;
  }

  static bool residencyStandard2DMultisampleBlockShape(
      const VkPhysicalDeviceSparseProperties& input) {
    return input.residencyStandard2DMultisampleBlockShape;
  }

  static bool residencyStandard3DBlockShape(
      const VkPhysicalDeviceSparseProperties& input) {
    return input.residencyStandard3DBlockShape;
  }

  static bool residencyAlignedMipSize(
      const VkPhysicalDeviceSparseProperties& input) {
    return input.residencyAlignedMipSize;
  }

  static bool residencyNonResidentStrict(
      const VkPhysicalDeviceSparseProperties& input) {
    return input.residencyNonResidentStrict;
  }

  static bool Read(gpu::mojom::VkPhysicalDeviceSparsePropertiesDataView data,
                   VkPhysicalDeviceSparseProperties* out);
};

template <>
struct StructTraits<gpu::mojom::VkPhysicalDeviceFeaturesDataView,
                    VkPhysicalDeviceFeatures> {
  static bool robustBufferAccess(const VkPhysicalDeviceFeatures& input) {
    return input.robustBufferAccess;
  }

  static bool fullDrawIndexUint32(const VkPhysicalDeviceFeatures& input) {
    return input.fullDrawIndexUint32;
  }

  static bool imageCubeArray(const VkPhysicalDeviceFeatures& input) {
    return input.imageCubeArray;
  }

  static bool independentBlend(const VkPhysicalDeviceFeatures& input) {
    return input.independentBlend;
  }

  static bool geometryShader(const VkPhysicalDeviceFeatures& input) {
    return input.geometryShader;
  }

  static bool tessellationShader(const VkPhysicalDeviceFeatures& input) {
    return input.tessellationShader;
  }

  static bool sampleRateShading(const VkPhysicalDeviceFeatures& input) {
    return input.sampleRateShading;
  }

  static bool dualSrcBlend(const VkPhysicalDeviceFeatures& input) {
    return input.dualSrcBlend;
  }

  static bool logicOp(const VkPhysicalDeviceFeatures& input) {
    return input.logicOp;
  }

  static bool multiDrawIndirect(const VkPhysicalDeviceFeatures& input) {
    return input.multiDrawIndirect;
  }

  static bool drawIndirectFirstInstance(const VkPhysicalDeviceFeatures& input) {
    return input.drawIndirectFirstInstance;
  }

  static bool depthClamp(const VkPhysicalDeviceFeatures& input) {
    return input.depthClamp;
  }

  static bool depthBiasClamp(const VkPhysicalDeviceFeatures& input) {
    return input.depthBiasClamp;
  }

  static bool fillModeNonSolid(const VkPhysicalDeviceFeatures& input) {
    return input.fillModeNonSolid;
  }

  static bool depthBounds(const VkPhysicalDeviceFeatures& input) {
    return input.depthBounds;
  }

  static bool wideLines(const VkPhysicalDeviceFeatures& input) {
    return input.wideLines;
  }

  static bool largePoints(const VkPhysicalDeviceFeatures& input) {
    return input.largePoints;
  }

  static bool alphaToOne(const VkPhysicalDeviceFeatures& input) {
    return input.alphaToOne;
  }

  static bool multiViewport(const VkPhysicalDeviceFeatures& input) {
    return input.multiViewport;
  }

  static bool samplerAnisotropy(const VkPhysicalDeviceFeatures& input) {
    return input.samplerAnisotropy;
  }

  static bool textureCompressionETC2(const VkPhysicalDeviceFeatures& input) {
    return input.textureCompressionETC2;
  }

  static bool textureCompressionASTC_LDR(
      const VkPhysicalDeviceFeatures& input) {
    return input.textureCompressionASTC_LDR;
  }

  static bool textureCompressionBC(const VkPhysicalDeviceFeatures& input) {
    return input.textureCompressionBC;
  }

  static bool occlusionQueryPrecise(const VkPhysicalDeviceFeatures& input) {
    return input.occlusionQueryPrecise;
  }

  static bool pipelineStatisticsQuery(const VkPhysicalDeviceFeatures& input) {
    return input.pipelineStatisticsQuery;
  }

  static bool vertexPipelineStoresAndAtomics(
      const VkPhysicalDeviceFeatures& input) {
    return input.vertexPipelineStoresAndAtomics;
  }

  static bool fragmentStoresAndAtomics(const VkPhysicalDeviceFeatures& input) {
    return input.fragmentStoresAndAtomics;
  }

  static bool shaderTessellationAndGeometryPointSize(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderTessellationAndGeometryPointSize;
  }

  static bool shaderImageGatherExtended(const VkPhysicalDeviceFeatures& input) {
    return input.shaderImageGatherExtended;
  }

  static bool shaderStorageImageExtendedFormats(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderStorageImageExtendedFormats;
  }

  static bool shaderStorageImageMultisample(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderStorageImageMultisample;
  }

  static bool shaderStorageImageReadWithoutFormat(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderStorageImageReadWithoutFormat;
  }

  static bool shaderStorageImageWriteWithoutFormat(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderStorageImageWriteWithoutFormat;
  }

  static bool shaderUniformBufferArrayDynamicIndexing(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderUniformBufferArrayDynamicIndexing;
  }

  static bool shaderSampledImageArrayDynamicIndexing(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderSampledImageArrayDynamicIndexing;
  }

  static bool shaderStorageBufferArrayDynamicIndexing(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderStorageBufferArrayDynamicIndexing;
  }

  static bool shaderStorageImageArrayDynamicIndexing(
      const VkPhysicalDeviceFeatures& input) {
    return input.shaderStorageImageArrayDynamicIndexing;
  }

  static bool shaderClipDistance(const VkPhysicalDeviceFeatures& input) {
    return input.shaderClipDistance;
  }

  static bool shaderCullDistance(const VkPhysicalDeviceFeatures& input) {
    return input.shaderCullDistance;
  }

  static bool shaderFloat64(const VkPhysicalDeviceFeatures& input) {
    return input.shaderFloat64;
  }

  static bool shaderInt64(const VkPhysicalDeviceFeatures& input) {
    return input.shaderInt64;
  }

  static bool shaderInt16(const VkPhysicalDeviceFeatures& input) {
    return input.shaderInt16;
  }

  static bool shaderResourceResidency(const VkPhysicalDeviceFeatures& input) {
    return input.shaderResourceResidency;
  }

  static bool shaderResourceMinLod(const VkPhysicalDeviceFeatures& input) {
    return input.shaderResourceMinLod;
  }

  static bool sparseBinding(const VkPhysicalDeviceFeatures& input) {
    return input.sparseBinding;
  }

  static bool sparseResidencyBuffer(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidencyBuffer;
  }

  static bool sparseResidencyImage2D(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidencyImage2D;
  }

  static bool sparseResidencyImage3D(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidencyImage3D;
  }

  static bool sparseResidency2Samples(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidency2Samples;
  }

  static bool sparseResidency4Samples(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidency4Samples;
  }

  static bool sparseResidency8Samples(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidency8Samples;
  }

  static bool sparseResidency16Samples(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidency16Samples;
  }

  static bool sparseResidencyAliased(const VkPhysicalDeviceFeatures& input) {
    return input.sparseResidencyAliased;
  }

  static bool variableMultisampleRate(const VkPhysicalDeviceFeatures& input) {
    return input.variableMultisampleRate;
  }

  static bool inheritedQueries(const VkPhysicalDeviceFeatures& input) {
    return input.inheritedQueries;
  }

  static bool Read(gpu::mojom::VkPhysicalDeviceFeaturesDataView data,
                   VkPhysicalDeviceFeatures* out);
};

template <>
struct StructTraits<gpu::mojom::VkQueueFamilyPropertiesDataView,
                    VkQueueFamilyProperties> {
  static VkQueueFlags queueFlags(const VkQueueFamilyProperties& input) {
    return input.queueFlags;
  }

  static uint32_t queueCount(const VkQueueFamilyProperties& input) {
    return input.queueCount;
  }

  static uint32_t timestampValidBits(const VkQueueFamilyProperties& input) {
    return input.timestampValidBits;
  }

  static const VkExtent3D& minImageTransferGranularity(
      const VkQueueFamilyProperties& input) {
    return input.minImageTransferGranularity;
  }

  static bool Read(gpu::mojom::VkQueueFamilyPropertiesDataView data,
                   VkQueueFamilyProperties* out);
};

template <>
struct StructTraits<gpu::mojom::VkExtent3DDataView, VkExtent3D> {
  static uint32_t width(const VkExtent3D& input) { return input.width; }

  static uint32_t height(const VkExtent3D& input) { return input.height; }

  static uint32_t depth(const VkExtent3D& input) { return input.depth; }

  static bool Read(gpu::mojom::VkExtent3DDataView data, VkExtent3D* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_VULKAN_TYPES_MOJOM_TRAITS_H_