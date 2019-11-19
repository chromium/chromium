// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/ipc/common/generate_vulkan_types.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/ipc/common/vulkan_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    gpu::mojom::VkExtensionPropertiesDataView,
    VkExtensionProperties>::Read(gpu::mojom::VkExtensionPropertiesDataView data,
                                 VkExtensionProperties* out) {
  base::StringPiece extensionName;
  if (!data.ReadExtensionName(&extensionName))
    return false;
  extensionName.copy(out->extensionName, sizeof(out->extensionName));

  out->specVersion = data.specVersion();

  return true;
}

// static
bool StructTraits<gpu::mojom::VkLayerPropertiesDataView, VkLayerProperties>::
    Read(gpu::mojom::VkLayerPropertiesDataView data, VkLayerProperties* out) {
  base::StringPiece layerName;
  if (!data.ReadLayerName(&layerName))
    return false;
  layerName.copy(out->layerName, sizeof(out->layerName));

  out->specVersion = data.specVersion();

  out->implementationVersion = data.implementationVersion();

  base::StringPiece description;
  if (!data.ReadDescription(&description))
    return false;
  description.copy(out->description, sizeof(out->description));

  return true;
}

// static
bool StructTraits<gpu::mojom::VkPhysicalDevicePropertiesDataView,
                  VkPhysicalDeviceProperties>::
    Read(gpu::mojom::VkPhysicalDevicePropertiesDataView data,
         VkPhysicalDeviceProperties* out) {
  out->apiVersion = data.apiVersion();

  out->driverVersion = data.driverVersion();

  out->vendorID = data.vendorID();

  out->deviceID = data.deviceID();

  if (!data.ReadDeviceType(&out->deviceType))
    return false;

  base::StringPiece deviceName;
  if (!data.ReadDeviceName(&deviceName))
    return false;
  deviceName.copy(out->deviceName, sizeof(out->deviceName));

  base::span<uint8_t> pipelineCacheUUID(out->pipelineCacheUUID);
  if (!data.ReadPipelineCacheUUID(&pipelineCacheUUID))
    return false;

  if (!data.ReadLimits(&out->limits))
    return false;

  if (!data.ReadSparseProperties(&out->sparseProperties))
    return false;

  return true;
}

// static
bool StructTraits<gpu::mojom::VkPhysicalDeviceLimitsDataView,
                  VkPhysicalDeviceLimits>::
    Read(gpu::mojom::VkPhysicalDeviceLimitsDataView data,
         VkPhysicalDeviceLimits* out) {
  out->maxImageDimension1D = data.maxImageDimension1D();

  out->maxImageDimension2D = data.maxImageDimension2D();

  out->maxImageDimension3D = data.maxImageDimension3D();

  out->maxImageDimensionCube = data.maxImageDimensionCube();

  out->maxImageArrayLayers = data.maxImageArrayLayers();

  out->maxTexelBufferElements = data.maxTexelBufferElements();

  out->maxUniformBufferRange = data.maxUniformBufferRange();

  out->maxStorageBufferRange = data.maxStorageBufferRange();

  out->maxPushConstantsSize = data.maxPushConstantsSize();

  out->maxMemoryAllocationCount = data.maxMemoryAllocationCount();

  out->maxSamplerAllocationCount = data.maxSamplerAllocationCount();

  out->bufferImageGranularity = data.bufferImageGranularity();

  out->sparseAddressSpaceSize = data.sparseAddressSpaceSize();

  out->maxBoundDescriptorSets = data.maxBoundDescriptorSets();

  out->maxPerStageDescriptorSamplers = data.maxPerStageDescriptorSamplers();

  out->maxPerStageDescriptorUniformBuffers =
      data.maxPerStageDescriptorUniformBuffers();

  out->maxPerStageDescriptorStorageBuffers =
      data.maxPerStageDescriptorStorageBuffers();

  out->maxPerStageDescriptorSampledImages =
      data.maxPerStageDescriptorSampledImages();

  out->maxPerStageDescriptorStorageImages =
      data.maxPerStageDescriptorStorageImages();

  out->maxPerStageDescriptorInputAttachments =
      data.maxPerStageDescriptorInputAttachments();

  out->maxPerStageResources = data.maxPerStageResources();

  out->maxDescriptorSetSamplers = data.maxDescriptorSetSamplers();

  out->maxDescriptorSetUniformBuffers = data.maxDescriptorSetUniformBuffers();

  out->maxDescriptorSetUniformBuffersDynamic =
      data.maxDescriptorSetUniformBuffersDynamic();

  out->maxDescriptorSetStorageBuffers = data.maxDescriptorSetStorageBuffers();

  out->maxDescriptorSetStorageBuffersDynamic =
      data.maxDescriptorSetStorageBuffersDynamic();

  out->maxDescriptorSetSampledImages = data.maxDescriptorSetSampledImages();

  out->maxDescriptorSetStorageImages = data.maxDescriptorSetStorageImages();

  out->maxDescriptorSetInputAttachments =
      data.maxDescriptorSetInputAttachments();

  out->maxVertexInputAttributes = data.maxVertexInputAttributes();

  out->maxVertexInputBindings = data.maxVertexInputBindings();

  out->maxVertexInputAttributeOffset = data.maxVertexInputAttributeOffset();

  out->maxVertexInputBindingStride = data.maxVertexInputBindingStride();

  out->maxVertexOutputComponents = data.maxVertexOutputComponents();

  out->maxTessellationGenerationLevel = data.maxTessellationGenerationLevel();

  out->maxTessellationPatchSize = data.maxTessellationPatchSize();

  out->maxTessellationControlPerVertexInputComponents =
      data.maxTessellationControlPerVertexInputComponents();

  out->maxTessellationControlPerVertexOutputComponents =
      data.maxTessellationControlPerVertexOutputComponents();

  out->maxTessellationControlPerPatchOutputComponents =
      data.maxTessellationControlPerPatchOutputComponents();

  out->maxTessellationControlTotalOutputComponents =
      data.maxTessellationControlTotalOutputComponents();

  out->maxTessellationEvaluationInputComponents =
      data.maxTessellationEvaluationInputComponents();

  out->maxTessellationEvaluationOutputComponents =
      data.maxTessellationEvaluationOutputComponents();

  out->maxGeometryShaderInvocations = data.maxGeometryShaderInvocations();

  out->maxGeometryInputComponents = data.maxGeometryInputComponents();

  out->maxGeometryOutputComponents = data.maxGeometryOutputComponents();

  out->maxGeometryOutputVertices = data.maxGeometryOutputVertices();

  out->maxGeometryTotalOutputComponents =
      data.maxGeometryTotalOutputComponents();

  out->maxFragmentInputComponents = data.maxFragmentInputComponents();

  out->maxFragmentOutputAttachments = data.maxFragmentOutputAttachments();

  out->maxFragmentDualSrcAttachments = data.maxFragmentDualSrcAttachments();

  out->maxFragmentCombinedOutputResources =
      data.maxFragmentCombinedOutputResources();

  out->maxComputeSharedMemorySize = data.maxComputeSharedMemorySize();

  base::span<uint32_t> maxComputeWorkGroupCount(out->maxComputeWorkGroupCount);
  if (!data.ReadMaxComputeWorkGroupCount(&maxComputeWorkGroupCount))
    return false;

  out->maxComputeWorkGroupInvocations = data.maxComputeWorkGroupInvocations();

  base::span<uint32_t> maxComputeWorkGroupSize(out->maxComputeWorkGroupSize);
  if (!data.ReadMaxComputeWorkGroupSize(&maxComputeWorkGroupSize))
    return false;

  out->subPixelPrecisionBits = data.subPixelPrecisionBits();

  out->subTexelPrecisionBits = data.subTexelPrecisionBits();

  out->mipmapPrecisionBits = data.mipmapPrecisionBits();

  out->maxDrawIndexedIndexValue = data.maxDrawIndexedIndexValue();

  out->maxDrawIndirectCount = data.maxDrawIndirectCount();

  out->maxSamplerLodBias = data.maxSamplerLodBias();

  out->maxSamplerAnisotropy = data.maxSamplerAnisotropy();

  out->maxViewports = data.maxViewports();

  base::span<uint32_t> maxViewportDimensions(out->maxViewportDimensions);
  if (!data.ReadMaxViewportDimensions(&maxViewportDimensions))
    return false;

  base::span<float> viewportBoundsRange(out->viewportBoundsRange);
  if (!data.ReadViewportBoundsRange(&viewportBoundsRange))
    return false;

  out->viewportSubPixelBits = data.viewportSubPixelBits();

  out->minMemoryMapAlignment = data.minMemoryMapAlignment();

  out->minTexelBufferOffsetAlignment = data.minTexelBufferOffsetAlignment();

  out->minUniformBufferOffsetAlignment = data.minUniformBufferOffsetAlignment();

  out->minStorageBufferOffsetAlignment = data.minStorageBufferOffsetAlignment();

  out->minTexelOffset = data.minTexelOffset();

  out->maxTexelOffset = data.maxTexelOffset();

  out->minTexelGatherOffset = data.minTexelGatherOffset();

  out->maxTexelGatherOffset = data.maxTexelGatherOffset();

  out->minInterpolationOffset = data.minInterpolationOffset();

  out->maxInterpolationOffset = data.maxInterpolationOffset();

  out->subPixelInterpolationOffsetBits = data.subPixelInterpolationOffsetBits();

  out->maxFramebufferWidth = data.maxFramebufferWidth();

  out->maxFramebufferHeight = data.maxFramebufferHeight();

  out->maxFramebufferLayers = data.maxFramebufferLayers();

  out->framebufferColorSampleCounts = data.framebufferColorSampleCounts();

  out->framebufferDepthSampleCounts = data.framebufferDepthSampleCounts();

  out->framebufferStencilSampleCounts = data.framebufferStencilSampleCounts();

  out->framebufferNoAttachmentsSampleCounts =
      data.framebufferNoAttachmentsSampleCounts();

  out->maxColorAttachments = data.maxColorAttachments();

  out->sampledImageColorSampleCounts = data.sampledImageColorSampleCounts();

  out->sampledImageIntegerSampleCounts = data.sampledImageIntegerSampleCounts();

  out->sampledImageDepthSampleCounts = data.sampledImageDepthSampleCounts();

  out->sampledImageStencilSampleCounts = data.sampledImageStencilSampleCounts();

  out->storageImageSampleCounts = data.storageImageSampleCounts();

  out->maxSampleMaskWords = data.maxSampleMaskWords();

  out->timestampComputeAndGraphics = data.timestampComputeAndGraphics();

  out->timestampPeriod = data.timestampPeriod();

  out->maxClipDistances = data.maxClipDistances();

  out->maxCullDistances = data.maxCullDistances();

  out->maxCombinedClipAndCullDistances = data.maxCombinedClipAndCullDistances();

  out->discreteQueuePriorities = data.discreteQueuePriorities();

  base::span<float> pointSizeRange(out->pointSizeRange);
  if (!data.ReadPointSizeRange(&pointSizeRange))
    return false;

  base::span<float> lineWidthRange(out->lineWidthRange);
  if (!data.ReadLineWidthRange(&lineWidthRange))
    return false;

  out->pointSizeGranularity = data.pointSizeGranularity();

  out->lineWidthGranularity = data.lineWidthGranularity();

  out->strictLines = data.strictLines();

  out->standardSampleLocations = data.standardSampleLocations();

  out->optimalBufferCopyOffsetAlignment =
      data.optimalBufferCopyOffsetAlignment();

  out->optimalBufferCopyRowPitchAlignment =
      data.optimalBufferCopyRowPitchAlignment();

  out->nonCoherentAtomSize = data.nonCoherentAtomSize();

  return true;
}

// static
bool StructTraits<gpu::mojom::VkPhysicalDeviceSparsePropertiesDataView,
                  VkPhysicalDeviceSparseProperties>::
    Read(gpu::mojom::VkPhysicalDeviceSparsePropertiesDataView data,
         VkPhysicalDeviceSparseProperties* out) {
  out->residencyStandard2DBlockShape = data.residencyStandard2DBlockShape();

  out->residencyStandard2DMultisampleBlockShape =
      data.residencyStandard2DMultisampleBlockShape();

  out->residencyStandard3DBlockShape = data.residencyStandard3DBlockShape();

  out->residencyAlignedMipSize = data.residencyAlignedMipSize();

  out->residencyNonResidentStrict = data.residencyNonResidentStrict();

  return true;
}

// static
bool StructTraits<gpu::mojom::VkPhysicalDeviceFeaturesDataView,
                  VkPhysicalDeviceFeatures>::
    Read(gpu::mojom::VkPhysicalDeviceFeaturesDataView data,
         VkPhysicalDeviceFeatures* out) {
  out->robustBufferAccess = data.robustBufferAccess();

  out->fullDrawIndexUint32 = data.fullDrawIndexUint32();

  out->imageCubeArray = data.imageCubeArray();

  out->independentBlend = data.independentBlend();

  out->geometryShader = data.geometryShader();

  out->tessellationShader = data.tessellationShader();

  out->sampleRateShading = data.sampleRateShading();

  out->dualSrcBlend = data.dualSrcBlend();

  out->logicOp = data.logicOp();

  out->multiDrawIndirect = data.multiDrawIndirect();

  out->drawIndirectFirstInstance = data.drawIndirectFirstInstance();

  out->depthClamp = data.depthClamp();

  out->depthBiasClamp = data.depthBiasClamp();

  out->fillModeNonSolid = data.fillModeNonSolid();

  out->depthBounds = data.depthBounds();

  out->wideLines = data.wideLines();

  out->largePoints = data.largePoints();

  out->alphaToOne = data.alphaToOne();

  out->multiViewport = data.multiViewport();

  out->samplerAnisotropy = data.samplerAnisotropy();

  out->textureCompressionETC2 = data.textureCompressionETC2();

  out->textureCompressionASTC_LDR = data.textureCompressionASTC_LDR();

  out->textureCompressionBC = data.textureCompressionBC();

  out->occlusionQueryPrecise = data.occlusionQueryPrecise();

  out->pipelineStatisticsQuery = data.pipelineStatisticsQuery();

  out->vertexPipelineStoresAndAtomics = data.vertexPipelineStoresAndAtomics();

  out->fragmentStoresAndAtomics = data.fragmentStoresAndAtomics();

  out->shaderTessellationAndGeometryPointSize =
      data.shaderTessellationAndGeometryPointSize();

  out->shaderImageGatherExtended = data.shaderImageGatherExtended();

  out->shaderStorageImageExtendedFormats =
      data.shaderStorageImageExtendedFormats();

  out->shaderStorageImageMultisample = data.shaderStorageImageMultisample();

  out->shaderStorageImageReadWithoutFormat =
      data.shaderStorageImageReadWithoutFormat();

  out->shaderStorageImageWriteWithoutFormat =
      data.shaderStorageImageWriteWithoutFormat();

  out->shaderUniformBufferArrayDynamicIndexing =
      data.shaderUniformBufferArrayDynamicIndexing();

  out->shaderSampledImageArrayDynamicIndexing =
      data.shaderSampledImageArrayDynamicIndexing();

  out->shaderStorageBufferArrayDynamicIndexing =
      data.shaderStorageBufferArrayDynamicIndexing();

  out->shaderStorageImageArrayDynamicIndexing =
      data.shaderStorageImageArrayDynamicIndexing();

  out->shaderClipDistance = data.shaderClipDistance();

  out->shaderCullDistance = data.shaderCullDistance();

  out->shaderFloat64 = data.shaderFloat64();

  out->shaderInt64 = data.shaderInt64();

  out->shaderInt16 = data.shaderInt16();

  out->shaderResourceResidency = data.shaderResourceResidency();

  out->shaderResourceMinLod = data.shaderResourceMinLod();

  out->sparseBinding = data.sparseBinding();

  out->sparseResidencyBuffer = data.sparseResidencyBuffer();

  out->sparseResidencyImage2D = data.sparseResidencyImage2D();

  out->sparseResidencyImage3D = data.sparseResidencyImage3D();

  out->sparseResidency2Samples = data.sparseResidency2Samples();

  out->sparseResidency4Samples = data.sparseResidency4Samples();

  out->sparseResidency8Samples = data.sparseResidency8Samples();

  out->sparseResidency16Samples = data.sparseResidency16Samples();

  out->sparseResidencyAliased = data.sparseResidencyAliased();

  out->variableMultisampleRate = data.variableMultisampleRate();

  out->inheritedQueries = data.inheritedQueries();

  return true;
}

// static
bool StructTraits<gpu::mojom::VkQueueFamilyPropertiesDataView,
                  VkQueueFamilyProperties>::
    Read(gpu::mojom::VkQueueFamilyPropertiesDataView data,
         VkQueueFamilyProperties* out) {
  out->queueFlags = data.queueFlags();

  out->queueCount = data.queueCount();

  out->timestampValidBits = data.timestampValidBits();

  if (!data.ReadMinImageTransferGranularity(&out->minImageTransferGranularity))
    return false;

  return true;
}

// static
bool StructTraits<gpu::mojom::VkExtent3DDataView, VkExtent3D>::Read(
    gpu::mojom::VkExtent3DDataView data,
    VkExtent3D* out) {
  out->width = data.width();

  out->height = data.height();

  out->depth = data.depth();

  return true;
}

}  // namespace mojo