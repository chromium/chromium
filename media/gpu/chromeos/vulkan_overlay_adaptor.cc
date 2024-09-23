// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/chromeos/vulkan_overlay_adaptor.h"

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "media/base/media_switches.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/shaders/shaders.h"
#include "media/gpu/macros.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

class VulkanOverlayAdaptor::VulkanRenderPass {
 public:
  VulkanRenderPass(const VulkanRenderPass&) = delete;
  VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

  ~VulkanRenderPass();

  static std::unique_ptr<VulkanRenderPass> Create(VkFormat format,
                                                  VkDevice logical_device);

  VkRenderPass Get();

 private:
  VulkanRenderPass(VkDevice logical_device, VkRenderPass render_pass);

  const VkRenderPass render_pass_;

  const VkDevice logical_device_;
};

class VulkanOverlayAdaptor::VulkanShader {
 public:
  VulkanShader(const VulkanShader&) = delete;
  VulkanShader& operator=(const VulkanShader&) = delete;

  ~VulkanShader();

  static std::unique_ptr<VulkanShader> Create(const uint32_t* shader_code,
                                              size_t shader_code_size,
                                              VkDevice logical_device);

  VkShaderModule Get();

 private:
  VulkanShader(VkDevice logical_device, VkShaderModule shader);

  const VkShaderModule shader_;

  const VkDevice logical_device_;
};

class VulkanOverlayAdaptor::VulkanPipeline {
 public:
  VulkanPipeline(const VulkanPipeline&) = delete;
  VulkanPipeline& operator=(const VulkanPipeline&) = delete;

  ~VulkanPipeline();

  static std::unique_ptr<VulkanPipeline> Create(
      const std::vector<VkVertexInputBindingDescription>& binding_descriptions,
      const std::vector<VkVertexInputAttributeDescription>&
          attribute_descriptions,
      const std::vector<VkDescriptorSetLayoutBinding>& ubo_bindings,
      std::unique_ptr<VulkanShader> vert_shader,
      std::unique_ptr<VulkanShader> frag_shader,
      const std::vector<size_t>& push_constants_size,
      VkRenderPass render_pass,
      VkDevice logical_device);

  VkPipeline Get();
  VkDescriptorSetLayout GetDescriptorSetLayout();
  VkPipelineLayout GetPipelineLayout();

 private:
  VulkanPipeline(VkPipeline pipeline,
                 VkDescriptorSetLayout descriptor_set_layout,
                 VkPipelineLayout pipeline_layout,
                 std::unique_ptr<VulkanShader> vert_shader,
                 std::unique_ptr<VulkanShader> frag_shader,
                 VkDevice logical_device);

  const VkPipeline pipeline_;
  const VkDescriptorSetLayout descriptor_set_layout_;
  const VkPipelineLayout pipeline_layout_;

  const std::unique_ptr<VulkanShader> vert_shader_;
  const std::unique_ptr<VulkanShader> frag_shader_;
  const VkDevice logical_device_;
};

class VulkanOverlayAdaptor::VulkanDescriptorPool {
 public:
  VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
  VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;

  ~VulkanDescriptorPool();

  static std::unique_ptr<VulkanDescriptorPool> Create(
      size_t num_descriptor_sets,
      std::vector<VkDescriptorType> descriptor_types,
      VkDescriptorSetLayout descriptor_set_layout,
      VkDevice logical_device);

  const std::vector<VkDescriptorSet>& Get();

 private:
  VulkanDescriptorPool(std::vector<VkDescriptorSet> descriptor_sets,
                       VkDescriptorPool descriptor_pool,
                       VkDevice logical_device);

  const std::vector<VkDescriptorSet> descriptor_sets_;

  const VkDescriptorPool descriptor_pool_;
  const VkDevice logical_device_;
};

class VulkanOverlayAdaptor::VulkanDeviceQueueWrapper {
 public:
  VulkanDeviceQueueWrapper(const VulkanDeviceQueueWrapper&) = delete;
  VulkanDeviceQueueWrapper& operator=(const VulkanDeviceQueueWrapper&) = delete;

  ~VulkanDeviceQueueWrapper();

  static std::unique_ptr<VulkanDeviceQueueWrapper> Create(
      gpu::VulkanImplementation* implementation);

  gpu::VulkanDeviceQueue* GetVulkanDeviceQueue();
  VkPhysicalDevice GetVulkanPhysicalDevice();
  VkPhysicalDeviceProperties GetVulkanPhysicalDeviceProperties();
  VkDevice GetVulkanDevice();
  VkQueue GetVulkanQueue();
  uint32_t GetVulkanQueueIndex();

 private:
  explicit VulkanDeviceQueueWrapper(
      std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue);

  const std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue_;
};

class VulkanOverlayAdaptor::VulkanCommandPoolWrapper {
 public:
  VulkanCommandPoolWrapper(const VulkanCommandPoolWrapper&) = delete;
  VulkanCommandPoolWrapper& operator=(const VulkanCommandPoolWrapper&) = delete;

  ~VulkanCommandPoolWrapper();

  static std::unique_ptr<VulkanCommandPoolWrapper> Create(
      gpu::VulkanDeviceQueue* device_queue,
      bool allow_protected_memory);

  std::unique_ptr<gpu::VulkanCommandBuffer> CreatePrimaryCommandBuffer();

 private:
  VulkanCommandPoolWrapper(
      std::unique_ptr<gpu::VulkanCommandPool> command_pool);

  const std::unique_ptr<gpu::VulkanCommandPool> command_pool_;
};

class VulkanOverlayAdaptor::VulkanTextureImage {
 public:
  VulkanTextureImage(const VulkanTextureImage&) = delete;
  VulkanTextureImage& operator=(const VulkanTextureImage&) = delete;

  ~VulkanTextureImage();

  static std::unique_ptr<VulkanTextureImage> Create(
      gpu::VulkanImage& image,
      const std::vector<VkFormat>& formats,
      const std::vector<gfx::Size>& sizes,
      const std::vector<VkImageAspectFlagBits>& aspects,
      bool is_framebuffer,
      VkRenderPass render_pass,
      VkDevice logical_device);

  VkImage GetImage();
  const std::vector<VkImageView>& GetImageViews();
  const std::vector<VkFramebuffer>& GetFramebuffers();
  void TransitionImageLayout(
      gpu::VulkanCommandBuffer* command_buf,
      VkImageLayout new_layout,
      uint32_t src_queue_family_index = VK_QUEUE_FAMILY_IGNORED,
      uint32_t dst_queue_family_index = VK_QUEUE_FAMILY_IGNORED);

 private:
  VulkanTextureImage(gpu::VulkanImage& image,
                     const std::vector<VkImageView>& image_views,
                     const std::vector<VkFramebuffer>& framebuffers,
                     VkImageLayout initial_layout,
                     VkDevice logical_device);

  const raw_ref<gpu::VulkanImage> image_;
  const std::vector<VkImageView> image_views_;
  const std::vector<VkFramebuffer> framebuffers_;

  VkImageLayout current_layout_;
  const VkDevice logical_device_;
};

class VulkanOverlayAdaptor::VulkanSampler {
 public:
  VulkanSampler(const VulkanSampler&) = delete;
  VulkanSampler& operator=(const VulkanSampler&) = delete;

  ~VulkanSampler();

  static std::unique_ptr<VulkanSampler> Create(VkFilter filter_mode,
                                               bool normalize_coords,
                                               VkDevice logical_device);

  VkSampler& Get();

 private:
  VulkanSampler(VkSampler sampler, VkDevice logical_device);

  VkSampler sampler_;

  const VkDevice logical_device_;
};

VulkanOverlayAdaptor::VulkanRenderPass::VulkanRenderPass(
    VkDevice logical_device,
    VkRenderPass render_pass)
    : render_pass_(render_pass), logical_device_(logical_device) {}

VulkanOverlayAdaptor::VulkanRenderPass::~VulkanRenderPass() {
  vkDestroyRenderPass(logical_device_, render_pass_, nullptr);
}

VkRenderPass VulkanOverlayAdaptor::VulkanRenderPass::Get() {
  return render_pass_;
}

std::unique_ptr<VulkanOverlayAdaptor::VulkanRenderPass>
VulkanOverlayAdaptor::VulkanRenderPass::Create(VkFormat format,
                                               VkDevice logical_device) {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;

  VkRenderPass render_pass;
  if (vkCreateRenderPass(logical_device, &render_pass_info, nullptr,
                         &render_pass) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create render pass!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanRenderPass(logical_device, render_pass));
}

VulkanOverlayAdaptor::VulkanShader::VulkanShader(VkDevice logical_device,
                                                 VkShaderModule shader)
    : shader_(shader), logical_device_(logical_device) {}

VulkanOverlayAdaptor::VulkanShader::~VulkanShader() {
  vkDestroyShaderModule(logical_device_, shader_, nullptr);
}

VkShaderModule VulkanOverlayAdaptor::VulkanShader::Get() {
  return shader_;
}

std::unique_ptr<VulkanOverlayAdaptor::VulkanShader>
VulkanOverlayAdaptor::VulkanShader::Create(const uint32_t* shader_code,
                                           size_t shader_code_size,
                                           VkDevice logical_device) {
  VkShaderModuleCreateInfo shader_info{};
  shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_info.codeSize = shader_code_size;
  shader_info.pCode = shader_code;

  VkShaderModule shader;
  if (vkCreateShaderModule(logical_device, &shader_info, nullptr, &shader) !=
      VK_SUCCESS) {
    LOG(ERROR) << "Could not create shader module!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanShader(logical_device, shader));
}

VulkanOverlayAdaptor::VulkanPipeline::VulkanPipeline(
    VkPipeline pipeline,
    VkDescriptorSetLayout descriptor_set_layout,
    VkPipelineLayout pipeline_layout,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanShader> vert_shader,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanShader> frag_shader,
    VkDevice logical_device)
    : pipeline_(pipeline),
      descriptor_set_layout_(descriptor_set_layout),
      pipeline_layout_(pipeline_layout),
      vert_shader_(std::move(vert_shader)),
      frag_shader_(std::move(frag_shader)),
      logical_device_(logical_device) {}

VulkanOverlayAdaptor::VulkanPipeline::~VulkanPipeline() {
  vkDestroyPipeline(logical_device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);
  vkDestroyDescriptorSetLayout(logical_device_, descriptor_set_layout_,
                               nullptr);
}

VkPipeline VulkanOverlayAdaptor::VulkanPipeline::Get() {
  return pipeline_;
}

VkDescriptorSetLayout
VulkanOverlayAdaptor::VulkanPipeline::GetDescriptorSetLayout() {
  return descriptor_set_layout_;
}

VkPipelineLayout VulkanOverlayAdaptor::VulkanPipeline::GetPipelineLayout() {
  return pipeline_layout_;
}

std::unique_ptr<VulkanOverlayAdaptor::VulkanPipeline>
VulkanOverlayAdaptor::VulkanPipeline::Create(
    const std::vector<VkVertexInputBindingDescription>& binding_descriptions,
    const std::vector<VkVertexInputAttributeDescription>&
        attribute_descriptions,
    const std::vector<VkDescriptorSetLayoutBinding>& ubo_bindings,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanShader> vert_shader,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanShader> frag_shader,
    const std::vector<size_t>& push_constants_size,
    VkRenderPass render_pass,
    VkDevice logical_device) {
  VkDescriptorSetLayoutCreateInfo descriptor_layout_info{};
  descriptor_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_layout_info.bindingCount = ubo_bindings.size();
  descriptor_layout_info.pBindings = ubo_bindings.data();

  VkDescriptorSetLayout descriptor_set_layout;
  if (vkCreateDescriptorSetLayout(logical_device, &descriptor_layout_info,
                                  nullptr,
                                  &descriptor_set_layout) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create descriptor set layout!";
    return nullptr;
  }

  std::vector<VkPushConstantRange> push_constant_range(
      push_constants_size.size());
  if (push_constants_size.size() > 0) {
    push_constant_range[0].offset = 0;
    push_constant_range[0].size =
        base::checked_cast<uint32_t>(push_constants_size[0]);
    push_constant_range[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  }
  if (push_constants_size.size() > 1) {
    push_constant_range[1].offset =
        base::checked_cast<uint32_t>(push_constants_size[0]);
    push_constant_range[1].size =
        base::checked_cast<uint32_t>(push_constants_size[1]);
    push_constant_range[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
  pipeline_layout_info.pPushConstantRanges = push_constant_range.data();
  pipeline_layout_info.pushConstantRangeCount = push_constants_size.size();

  VkPipelineLayout pipeline_layout;
  if (vkCreatePipelineLayout(logical_device, &pipeline_layout_info, nullptr,
                             &pipeline_layout) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create pipeline layout!";
    return nullptr;
  }

  VkPipelineShaderStageCreateInfo shader_stages[2] = {};

  VkPipelineShaderStageCreateInfo& vert_shader_info = shader_stages[0];
  vert_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_shader_info.module = vert_shader->Get();
  vert_shader_info.pName = "main";

  VkPipelineShaderStageCreateInfo& frag_shader_info = shader_stages[1];
  frag_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_shader_info.module = frag_shader->Get();
  frag_shader_info.pName = "main";

  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = binding_descriptions.size();
  vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
  vertex_input_info.vertexAttributeDescriptionCount =
      attribute_descriptions.size();
  vertex_input_info.pVertexAttributeDescriptions =
      attribute_descriptions.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = 1.0;
  viewport.height = 1.0;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {1, 1};

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = nullptr;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline;
  if (vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1,
                                &pipeline_info, nullptr,
                                &pipeline) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create graphics pipeline!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanPipeline(
      pipeline, descriptor_set_layout, pipeline_layout, std::move(vert_shader),
      std::move(frag_shader), logical_device));
}

VulkanOverlayAdaptor::VulkanDescriptorPool::VulkanDescriptorPool(
    std::vector<VkDescriptorSet> descriptor_sets,
    VkDescriptorPool descriptor_pool,
    VkDevice logical_device)
    : descriptor_sets_(descriptor_sets),
      descriptor_pool_(descriptor_pool),
      logical_device_(logical_device) {}

VulkanOverlayAdaptor::VulkanDescriptorPool::~VulkanDescriptorPool() {
  vkDestroyDescriptorPool(logical_device_, descriptor_pool_, nullptr);
}

const std::vector<VkDescriptorSet>&
VulkanOverlayAdaptor::VulkanDescriptorPool::Get() {
  return descriptor_sets_;
}

std::unique_ptr<VulkanOverlayAdaptor::VulkanDescriptorPool>
VulkanOverlayAdaptor::VulkanDescriptorPool::Create(
    size_t num_descriptor_sets,
    std::vector<VkDescriptorType> descriptor_types,
    VkDescriptorSetLayout descriptor_set_layout,
    VkDevice logical_device) {
  std::vector<VkDescriptorPoolSize> pool_sizes;
  for (auto& type : descriptor_types) {
    VkDescriptorPoolSize pool_size;
    pool_size.type = type;
    pool_size.descriptorCount = num_descriptor_sets;
    pool_sizes.push_back(pool_size);
  }

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = pool_sizes.size();
  pool_info.pPoolSizes = pool_sizes.data();
  pool_info.maxSets = num_descriptor_sets;

  VkDescriptorPool descriptor_pool;
  if (vkCreateDescriptorPool(logical_device, &pool_info, nullptr,
                             &descriptor_pool) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create descriptor pool!";
    return nullptr;
  }

  std::vector<VkDescriptorSetLayout> layouts(num_descriptor_sets,
                                             descriptor_set_layout);
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool;
  alloc_info.descriptorSetCount = num_descriptor_sets;
  alloc_info.pSetLayouts = layouts.data();

  std::vector<VkDescriptorSet> descriptor_sets(num_descriptor_sets);
  if (vkAllocateDescriptorSets(logical_device, &alloc_info,
                               descriptor_sets.data()) != VK_SUCCESS) {
    LOG(ERROR) << "Could not create descriptor sets!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanDescriptorPool(
      descriptor_sets, descriptor_pool, logical_device));
}

VulkanOverlayAdaptor::VulkanTextureImage::VulkanTextureImage(
    gpu::VulkanImage& image,
    const std::vector<VkImageView>& image_views,
    const std::vector<VkFramebuffer>& framebuffers,
    VkImageLayout initial_layout,
    VkDevice logical_device)
    : image_(image),
      image_views_(image_views),
      framebuffers_(framebuffers),
      current_layout_(initial_layout),
      logical_device_(logical_device) {}

VulkanOverlayAdaptor::VulkanTextureImage::~VulkanTextureImage() {
  for (VkFramebuffer framebuffer : framebuffers_) {
    vkDestroyFramebuffer(logical_device_, framebuffer, nullptr);
  }

  for (VkImageView image_view : image_views_) {
    vkDestroyImageView(logical_device_, image_view, nullptr);
  }
}

VkImage VulkanOverlayAdaptor::VulkanTextureImage::GetImage() {
  return image_->image();
}

const std::vector<VkImageView>&
VulkanOverlayAdaptor::VulkanTextureImage::GetImageViews() {
  return image_views_;
}

const std::vector<VkFramebuffer>&
VulkanOverlayAdaptor::VulkanTextureImage::GetFramebuffers() {
  return framebuffers_;
}

void VulkanOverlayAdaptor::VulkanTextureImage::TransitionImageLayout(
    gpu::VulkanCommandBuffer* command_buf,
    VkImageLayout new_layout,
    uint32_t src_queue_family_index,
    uint32_t dst_queue_family_index) {
  if (new_layout == current_layout_) {
    return;
  }

  command_buf->TransitionImageLayout(image_->image(), current_layout_,
                                     new_layout, src_queue_family_index,
                                     dst_queue_family_index);

  current_layout_ = new_layout;
}

std::unique_ptr<VulkanOverlayAdaptor::VulkanTextureImage>
VulkanOverlayAdaptor::VulkanTextureImage::Create(
    gpu::VulkanImage& image,
    const std::vector<VkFormat>& formats,
    const std::vector<gfx::Size>& sizes,
    const std::vector<VkImageAspectFlagBits>& aspects,
    bool is_framebuffer,
    VkRenderPass render_pass,
    VkDevice logical_device) {
  CHECK_EQ(formats.size(), sizes.size());
  CHECK_EQ(sizes.size(), aspects.size());

  std::vector<VkImageView> image_views;
  for (size_t i = 0; i < formats.size(); i++) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image.image();
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = formats[i];
    view_info.subresourceRange.aspectMask = aspects[i];
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(logical_device, &view_info, nullptr, &image_view) !=
        VK_SUCCESS) {
      LOG(ERROR) << "Could not create image view!";
      return nullptr;
    }

    image_views.emplace_back(std::move(image_view));
  }

  std::vector<VkFramebuffer> framebuffers;
  if (is_framebuffer) {
    for (size_t i = 0; i < sizes.size(); i++) {
      VkFramebufferCreateInfo framebuffer_info{};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = render_pass;
      framebuffer_info.attachmentCount = 1;
      framebuffer_info.pAttachments = image_views.data() + i;
      framebuffer_info.width = sizes[i].width();
      framebuffer_info.height = sizes[i].height();
      framebuffer_info.layers = 1;

      VkFramebuffer framebuffer;
      if (vkCreateFramebuffer(logical_device, &framebuffer_info, nullptr,
                              &framebuffer) != VK_SUCCESS) {
        LOG(ERROR) << "Could not create framebuffer!";
      }

      framebuffers.emplace_back(std::move(framebuffer));
    }
  }

  return base::WrapUnique(new VulkanTextureImage(
      image, std::move(image_views), std::move(framebuffers),
      is_framebuffer ? VK_IMAGE_LAYOUT_UNDEFINED
                     : VK_IMAGE_LAYOUT_PREINITIALIZED,
      logical_device));
}

VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::VulkanDeviceQueueWrapper(
    std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue)
    : vulkan_device_queue_(std::move(vulkan_device_queue)) {}

VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::~VulkanDeviceQueueWrapper() {
  vulkan_device_queue_->Destroy();
}

gpu::VulkanDeviceQueue*
VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::GetVulkanDeviceQueue() {
  return vulkan_device_queue_.get();
}

VkPhysicalDevice
VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::GetVulkanPhysicalDevice() {
  return vulkan_device_queue_->GetVulkanPhysicalDevice();
}

VkPhysicalDeviceProperties VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::
    GetVulkanPhysicalDeviceProperties() {
  return vulkan_device_queue_->vk_physical_device_properties();
}

VkDevice VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::GetVulkanDevice() {
  return vulkan_device_queue_->GetVulkanDevice();
}

VkQueue VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::GetVulkanQueue() {
  return vulkan_device_queue_->GetVulkanQueue();
}

uint32_t VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::GetVulkanQueueIndex() {
  return vulkan_device_queue_->GetVulkanQueueIndex();
}

std::unique_ptr<VulkanOverlayAdaptor::VulkanDeviceQueueWrapper>
VulkanOverlayAdaptor::VulkanDeviceQueueWrapper::Create(
    gpu::VulkanImplementation* implementation) {
  auto vulkan_device_queue = CreateVulkanDeviceQueue(
      implementation,
      gpu::VulkanDeviceQueue::DeviceQueueOption::GRAPHICS_QUEUE_FLAG);

  if (!vulkan_device_queue) {
    LOG(ERROR) << "Could not create VulkanDeviceQueue";
    return nullptr;
  }

  return base::WrapUnique(
      new VulkanDeviceQueueWrapper(std::move(vulkan_device_queue)));
}

VulkanOverlayAdaptor::VulkanCommandPoolWrapper::VulkanCommandPoolWrapper(
    std::unique_ptr<gpu::VulkanCommandPool> command_pool)
    : command_pool_(std::move(command_pool)) {}

VulkanOverlayAdaptor::VulkanCommandPoolWrapper::~VulkanCommandPoolWrapper() {
  command_pool_->Destroy();
}

std::unique_ptr<gpu::VulkanCommandBuffer>
VulkanOverlayAdaptor::VulkanCommandPoolWrapper::CreatePrimaryCommandBuffer() {
  return command_pool_->CreatePrimaryCommandBuffer();
}

std::unique_ptr<VulkanOverlayAdaptor::VulkanCommandPoolWrapper>
VulkanOverlayAdaptor::VulkanCommandPoolWrapper::Create(
    gpu::VulkanDeviceQueue* device_queue,
    bool allow_protected_memory) {
  std::unique_ptr<gpu::VulkanCommandPool> command_pool =
      base::WrapUnique(new gpu::VulkanCommandPool(device_queue));
  command_pool->Initialize(allow_protected_memory);

  return base::WrapUnique(
      new VulkanCommandPoolWrapper(std::move(command_pool)));
}

VulkanOverlayAdaptor::VulkanSampler::VulkanSampler(VkSampler sampler,
                                                   VkDevice logical_device)
    : sampler_(sampler), logical_device_(logical_device) {}

std::unique_ptr<VulkanOverlayAdaptor::VulkanSampler>
VulkanOverlayAdaptor::VulkanSampler::Create(VkFilter filter_mode,
                                            bool normalize_coords,
                                            VkDevice logical_device) {
  VkSamplerCreateInfo sampler_info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = filter_mode,
      .minFilter = filter_mode,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = !normalize_coords,
  };

  VkSampler sampler;
  if (vkCreateSampler(logical_device, &sampler_info, nullptr, &sampler) !=
      VK_SUCCESS) {
    LOG(ERROR) << "Could not create sampler!";
    return nullptr;
  }

  return base::WrapUnique(new VulkanSampler(sampler, logical_device));
}

VkSampler& VulkanOverlayAdaptor::VulkanSampler::Get() {
  return sampler_;
}

VulkanOverlayAdaptor::VulkanSampler::~VulkanSampler() {
  vkDestroySampler(logical_device_, sampler_, nullptr);
}

VulkanOverlayAdaptor::VulkanOverlayAdaptor(
    std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanDeviceQueueWrapper>
        vulkan_device_queue,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanCommandPoolWrapper>
        command_pool,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanRenderPass> convert_render_pass,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanRenderPass>
        transform_render_pass,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanPipeline> convert_pipeline,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanPipeline> transform_pipeline,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanDescriptorPool>
        convert_descriptor_pool,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanDescriptorPool>
        transform_descriptor_pool,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanSampler> sampler,
    std::unique_ptr<gpu::VulkanImage> pivot_image,
    std::unique_ptr<VulkanOverlayAdaptor::VulkanTextureImage> pivot_texture,
    bool is_protected,
    TiledImageFormat tile_format)
    : vulkan_implementation_(std::move(vulkan_implementation)),
      vulkan_device_queue_(std::move(vulkan_device_queue)),
      command_pool_(std::move(command_pool)),
      convert_render_pass_(std::move(convert_render_pass)),
      transform_render_pass_(std::move(transform_render_pass)),
      convert_pipeline_(std::move(convert_pipeline)),
      transform_pipeline_(std::move(transform_pipeline)),
      convert_descriptor_pool_(std::move(convert_descriptor_pool)),
      transform_descriptor_pool_(std::move(transform_descriptor_pool)),
      sampler_(std::move(sampler)),
      pivot_image_(std::move(pivot_image)),
      pivot_texture_(std::move(pivot_texture)),
      is_protected_(is_protected),
      tile_format_(tile_format) {}

VulkanOverlayAdaptor::~VulkanOverlayAdaptor() {
  // Make sure there aren't any pending cleanup jobs before we start destroying
  // stuff.
  vulkan_device_queue_->GetVulkanDeviceQueue()
      ->GetFenceHelper()
      ->PerformImmediateCleanup();

  pivot_image_->Destroy();
}

std::unique_ptr<VulkanOverlayAdaptor> VulkanOverlayAdaptor::Create(
    bool is_protected,
    TiledImageFormat format,
    const gfx::Size& max_size) {
  auto vulkan_implementation = gpu::CreateVulkanImplementation(
      /*use_swiftshader=*/false, is_protected);

  if (!vulkan_implementation->InitializeVulkanInstance(
          /*using_surface=*/false)) {
    LOG(ERROR) << "Error initializing Vulkan instance";
    return nullptr;
  }

  auto vulkan_device_queue =
      VulkanDeviceQueueWrapper::Create(vulkan_implementation.get());
  if (!vulkan_device_queue) {
    return nullptr;
  }

  auto command_pool = VulkanCommandPoolWrapper::Create(
      vulkan_device_queue->GetVulkanDeviceQueue(), is_protected);
  if (!command_pool) {
    return nullptr;
  }

  VkFormat out_format =
      (format == kMT2T
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION) && \
    defined(ARCH_CPU_ARM_FAMILY)
       && base::FeatureList::IsEnabled(media::kEnableArmHwdrm10bitOverlays)
#endif
           )
          ? VK_FORMAT_A2R10G10B10_UNORM_PACK32
          : VK_FORMAT_B8G8R8A8_UNORM;
  auto convert_render_pass = VulkanRenderPass::Create(
      out_format, vulkan_device_queue->GetVulkanDevice());
  if (!convert_render_pass) {
    return nullptr;
  }
  auto transform_render_pass = VulkanRenderPass::Create(
      out_format, vulkan_device_queue->GetVulkanDevice());
  if (!transform_render_pass) {
    return nullptr;
  }

  std::unique_ptr<VulkanShader> convert_vert_shader = nullptr;
  if (format == kMM21) {
    convert_vert_shader =
        VulkanShader::Create(kMM21ShaderVert, sizeof(kMM21ShaderVert),
                             vulkan_device_queue->GetVulkanDevice());
  } else {
    convert_vert_shader =
        VulkanShader::Create(kMT2TShaderVert, sizeof(kMT2TShaderVert),
                             vulkan_device_queue->GetVulkanDevice());
  }
  if (!convert_vert_shader) {
    return nullptr;
  }
  auto transform_vert_shader =
      VulkanShader::Create(kCropRotateShaderVert, sizeof(kCropRotateShaderVert),
                           vulkan_device_queue->GetVulkanDevice());
  if (!transform_vert_shader) {
    return nullptr;
  }

  std::unique_ptr<VulkanShader> convert_frag_shader = nullptr;
  if (format == kMM21) {
    convert_frag_shader =
        VulkanShader::Create(kMM21ShaderFrag, sizeof(kMM21ShaderFrag),
                             vulkan_device_queue->GetVulkanDevice());
  } else {
    convert_frag_shader =
        VulkanShader::Create(kMT2TShaderFrag, sizeof(kMT2TShaderFrag),
                             vulkan_device_queue->GetVulkanDevice());
  }
  if (!convert_frag_shader) {
    return nullptr;
  }
  auto transform_frag_shader =
      VulkanShader::Create(kCropRotateShaderFrag, sizeof(kCropRotateShaderFrag),
                           vulkan_device_queue->GetVulkanDevice());
  if (!transform_frag_shader) {
    return nullptr;
  }

  std::vector<VkVertexInputBindingDescription> binding_descriptions;
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

  std::vector<VkDescriptorSetLayoutBinding> descriptor_bindings(1);
  descriptor_bindings[0].binding = 0;
  descriptor_bindings[0].descriptorCount = 1;
  descriptor_bindings[0].descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptor_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  auto transform_pipeline = VulkanPipeline::Create(
      binding_descriptions, attribute_descriptions, descriptor_bindings,
      std::move(transform_vert_shader), std::move(transform_frag_shader),
      {6 * 2 * sizeof(float) + 2 * sizeof(float)}, transform_render_pass->Get(),
      vulkan_device_queue->GetVulkanDevice());
  if (!transform_pipeline) {
    return nullptr;
  }

  descriptor_bindings.emplace_back();
  descriptor_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_bindings[1].binding = 1;
  descriptor_bindings[1].descriptorCount = 1;
  descriptor_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  auto convert_pipeline = VulkanPipeline::Create(
      binding_descriptions, attribute_descriptions, descriptor_bindings,
      std::move(convert_vert_shader), std::move(convert_frag_shader),
      {3 * 2 * sizeof(float), 2 * sizeof(float)}, convert_render_pass->Get(),
      vulkan_device_queue->GetVulkanDevice());
  if (!convert_pipeline) {
    return nullptr;
  }

  auto convert_descriptor_pool = VulkanDescriptorPool::Create(
      1, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE},
      convert_pipeline->GetDescriptorSetLayout(),
      vulkan_device_queue->GetVulkanDevice());
  if (!convert_descriptor_pool) {
    return nullptr;
  }
  auto transform_descriptor_pool = VulkanDescriptorPool::Create(
      1, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
      transform_pipeline->GetDescriptorSetLayout(),
      vulkan_device_queue->GetVulkanDevice());
  if (!convert_descriptor_pool) {
    return nullptr;
  }

  auto sampler = VulkanSampler::Create(/*filter_mode=*/VK_FILTER_LINEAR,
                                       /*normalize_coords=*/true,
                                       vulkan_device_queue->GetVulkanDevice());
  if (!sampler) {
    return nullptr;
  }

  auto pivot_image = gpu::VulkanImage::Create(
      vulkan_device_queue->GetVulkanDeviceQueue(), max_size, out_format,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      is_protected ? VK_IMAGE_CREATE_PROTECTED_BIT : 0,
      VK_IMAGE_TILING_OPTIMAL);
  auto pivot_texture = VulkanTextureImage::Create(
      *pivot_image, {out_format}, {pivot_image->size()},
      {VK_IMAGE_ASPECT_COLOR_BIT},
      /*is_framebuffer=*/true, convert_render_pass->Get(),
      vulkan_device_queue->GetVulkanDevice());

  return base::WrapUnique(new VulkanOverlayAdaptor(
      std::move(vulkan_implementation), std::move(vulkan_device_queue),
      std::move(command_pool), std::move(convert_render_pass),
      std::move(transform_render_pass), std::move(convert_pipeline),
      std::move(transform_pipeline), std::move(convert_descriptor_pool),
      std::move(transform_descriptor_pool), std::move(sampler),
      std::move(pivot_image), std::move(pivot_texture), is_protected, format));
}

void VulkanOverlayAdaptor::Process(gpu::VulkanImage& in_image,
                                   const gfx::Size& input_visible_size,
                                   gpu::VulkanImage& out_image,
                                   const gfx::RectF& display_rect,
                                   const gfx::RectF& crop_rect,
                                   gfx::OverlayTransform transform,
                                   std::vector<VkSemaphore>& begin_semaphores,
                                   std::vector<VkSemaphore>& end_semaphores) {
  CHECK(crop_rect.width() <= 1.0f && crop_rect.width() >= 0.0f);
  CHECK(crop_rect.height() <= 1.0f && crop_rect.height() >= 0.0f);
  CHECK(crop_rect.x() <= 1.0f && crop_rect.x() >= 0.0f);
  CHECK(crop_rect.y() <= 1.0f && crop_rect.y() >= 0.0f);
  CHECK(in_image.format() == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
  CHECK(out_image.format() == VK_FORMAT_B8G8R8A8_UNORM ||
        out_image.format() == VK_FORMAT_A2R10G10B10_UNORM_PACK32);
  constexpr size_t kMM21TileWidth = 16;
  constexpr size_t kMM21TileHeight = 32;
  const gfx::Size input_coded_size(
      base::bits::AlignUp(static_cast<size_t>(input_visible_size.width()),
                          kMM21TileWidth),
      base::bits::AlignUp(static_cast<size_t>(input_visible_size.height()),
                          kMM21TileHeight));

  const gfx::Size output_resolution(
      static_cast<int>(display_rect.width() / crop_rect.width()),
      static_cast<int>(display_rect.height() / crop_rect.height()));

  // TODO(b/251458823): Investigate whether it's more efficient to change the
  // vertex coordinates or the UV coordinates. The latter may optimize for
  // contiguous writes over contiguous reads, which takes better advantage of
  // the write combiner.
  float x_start = -1.0f - 2.0f * crop_rect.x();
  float x_end = 1.0f - 2.0f * crop_rect.x();
  float y_start = -1.0f - 2.0f * crop_rect.y();
  float y_end = 1.0f - 2.0f * crop_rect.y();
  float vertex_push_constants[14] = {0};
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      vertex_push_constants[0] = x_end;
      vertex_push_constants[1] = y_start;
      vertex_push_constants[2] = x_end;
      vertex_push_constants[3] = y_end;
      vertex_push_constants[4] = x_start;
      vertex_push_constants[5] = y_start;
      vertex_push_constants[6] = x_start;
      vertex_push_constants[7] = y_start;
      vertex_push_constants[8] = x_end;
      vertex_push_constants[9] = y_end;
      vertex_push_constants[10] = x_start;
      vertex_push_constants[11] = y_end;
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      vertex_push_constants[0] = x_end;
      vertex_push_constants[1] = y_end;
      vertex_push_constants[2] = x_start;
      vertex_push_constants[3] = y_end;
      vertex_push_constants[4] = x_end;
      vertex_push_constants[5] = y_start;
      vertex_push_constants[6] = x_end;
      vertex_push_constants[7] = y_start;
      vertex_push_constants[8] = x_start;
      vertex_push_constants[9] = y_end;
      vertex_push_constants[10] = x_start;
      vertex_push_constants[11] = y_start;
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      vertex_push_constants[0] = x_start;
      vertex_push_constants[1] = y_end;
      vertex_push_constants[2] = x_start;
      vertex_push_constants[3] = y_start;
      vertex_push_constants[4] = x_end;
      vertex_push_constants[5] = y_end;
      vertex_push_constants[6] = x_end;
      vertex_push_constants[7] = y_end;
      vertex_push_constants[8] = x_start;
      vertex_push_constants[9] = y_start;
      vertex_push_constants[10] = x_end;
      vertex_push_constants[11] = y_start;
      break;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      vertex_push_constants[0] = x_start;
      vertex_push_constants[1] = y_start;
      vertex_push_constants[2] = x_end;
      vertex_push_constants[3] = y_start;
      vertex_push_constants[4] = x_start;
      vertex_push_constants[5] = y_end;
      vertex_push_constants[6] = x_start;
      vertex_push_constants[7] = y_end;
      vertex_push_constants[8] = x_end;
      vertex_push_constants[9] = y_start;
      vertex_push_constants[10] = x_end;
      vertex_push_constants[11] = y_end;
      break;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
    default:
      LOG(ERROR) << "Unsupported rotation requested for VulkanOverlayAdaptor.";
      return;
  }
  vertex_push_constants[12] = static_cast<float>(input_visible_size.width()) /
                              static_cast<float>(pivot_image_->size().width());
  vertex_push_constants[13] = static_cast<float>(input_visible_size.height()) /
                              static_cast<float>(pivot_image_->size().height());

  auto out_texture = VulkanTextureImage::Create(
      out_image, {out_image.format()}, {output_resolution},
      {VK_IMAGE_ASPECT_COLOR_BIT},
      /*is_framebuffer=*/true, transform_render_pass_->Get(),
      vulkan_device_queue_->GetVulkanDevice());

  gfx::Size uv_plane_size = gfx::Size((input_coded_size.width() + 1) / 2,
                                      (input_coded_size.height() + 1) / 2);
  auto in_texture = VulkanTextureImage::Create(
      in_image, {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM},
      {input_coded_size, uv_plane_size},
      {VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_ASPECT_PLANE_1_BIT},
      /*is_framebuffer=*/false, convert_render_pass_->Get(),
      vulkan_device_queue_->GetVulkanDevice());

  std::array<VkWriteDescriptorSet, 3> descriptor_write;

  std::array<VkDescriptorImageInfo, 3> image_info;
  image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info[0].imageView = in_texture->GetImageViews()[0];
  image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info[1].imageView = in_texture->GetImageViews()[1];
  image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info[2].imageView = pivot_texture_->GetImageViews()[0];
  image_info[2].sampler = sampler_->Get();
  descriptor_write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write[0].dstSet = convert_descriptor_pool_->Get()[0];
  descriptor_write[0].dstBinding = 0;
  descriptor_write[0].dstArrayElement = 0;
  descriptor_write[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_write[0].descriptorCount = 1;
  descriptor_write[0].pImageInfo = image_info.data();
  descriptor_write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write[1].dstSet = convert_descriptor_pool_->Get()[0];
  descriptor_write[1].dstBinding = 1;
  descriptor_write[1].dstArrayElement = 0;
  descriptor_write[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  descriptor_write[1].descriptorCount = 1;
  descriptor_write[1].pImageInfo = image_info.data() + 1;
  descriptor_write[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write[2].dstSet = transform_descriptor_pool_->Get()[0];
  descriptor_write[2].dstBinding = 0;
  descriptor_write[2].dstArrayElement = 0;
  descriptor_write[2].descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptor_write[2].descriptorCount = 1;
  descriptor_write[2].pImageInfo = image_info.data() + 2;

  vkUpdateDescriptorSets(vulkan_device_queue_->GetVulkanDevice(),
                         descriptor_write.size(), descriptor_write.data(), 0,
                         nullptr);

  auto command_buf = command_pool_->CreatePrimaryCommandBuffer();
  {
    gpu::ScopedSingleUseCommandBufferRecorder record(*command_buf);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(input_coded_size.width());
    viewport.height = static_cast<float>(input_coded_size.height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    CHECK(viewport.width <= 10000.0f && viewport.width >= 0.0f);
    CHECK(viewport.height <= 10000.0f && viewport.height >= 0.0f);
    vkCmdSetViewport(record.handle(), 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = static_cast<uint32_t>(input_coded_size.width());
    scissor.extent.height = static_cast<uint32_t>(input_coded_size.height());
    vkCmdSetScissor(record.handle(), 0, 1, &scissor);

    in_texture->TransitionImageLayout(command_buf.get(),
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    pivot_texture_->TransitionImageLayout(
        command_buf.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = convert_render_pass_->Get();
    render_pass_info.framebuffer = pivot_texture_->GetFramebuffers()[0];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = {
        base::checked_cast<uint32_t>(input_coded_size.width()),
        base::checked_cast<uint32_t>(input_coded_size.height())};
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;
    vkCmdBeginRenderPass(record.handle(), &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(record.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                      convert_pipeline_->Get());

    vkCmdBindDescriptorSets(record.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            convert_pipeline_->GetPipelineLayout(), 0, 1,
                            convert_descriptor_pool_->Get().data(), 0, nullptr);

    float convert_vert_constants[6] = {
        static_cast<float>(input_coded_size.width() / kMM21TileWidth), 0.0,
        static_cast<float>(input_coded_size.width()),
        static_cast<float>(input_coded_size.height()),
        static_cast<float>(input_coded_size.width()),
        static_cast<float>(input_coded_size.width() / 2)};
    vkCmdPushConstants(record.handle(), convert_pipeline_->GetPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(convert_vert_constants), convert_vert_constants);

    float convert_frag_constants[2] = {
        static_cast<float>(input_coded_size.width()),
        static_cast<float>(input_coded_size.width() / 2)};
    vkCmdPushConstants(record.handle(), convert_pipeline_->GetPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(convert_vert_constants),
                       sizeof(convert_frag_constants), convert_frag_constants);

    int num_vertices =
        input_coded_size.GetArea() / (kMM21TileWidth * kMM21TileHeight) * 6;
    vkCmdDraw(record.handle(), num_vertices, 1, 0, 0);

    vkCmdEndRenderPass(record.handle());

    pivot_texture_->TransitionImageLayout(
        command_buf.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    viewport.width = static_cast<float>(output_resolution.width());
    viewport.height = static_cast<float>(output_resolution.height());
    vkCmdSetViewport(record.handle(), 0, 1, &viewport);

    scissor.extent.width =
        static_cast<uint32_t>(output_resolution.width() * crop_rect.width());
    scissor.extent.height =
        static_cast<uint32_t>(output_resolution.height() * crop_rect.height());
    vkCmdSetScissor(record.handle(), 0, 1, &scissor);

    render_pass_info.renderPass = transform_render_pass_->Get();
    render_pass_info.framebuffer = out_texture->GetFramebuffers()[0];
    render_pass_info.renderArea.extent = {
        base::checked_cast<uint32_t>(output_resolution.width()),
        base::checked_cast<uint32_t>(output_resolution.height())};
    vkCmdBeginRenderPass(record.handle(), &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(record.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                      transform_pipeline_->Get());

    vkCmdBindDescriptorSets(record.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            transform_pipeline_->GetPipelineLayout(), 0, 1,
                            transform_descriptor_pool_->Get().data(), 0,
                            nullptr);

    vkCmdPushConstants(record.handle(),
                       transform_pipeline_->GetPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(vertex_push_constants), vertex_push_constants);

    vkCmdDraw(record.handle(), 6, 1, 0, 0);

    vkCmdEndRenderPass(record.handle());

    out_texture->TransitionImageLayout(
        command_buf.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_FOREIGN_EXT);
  }

  auto* fence_helper =
      vulkan_device_queue_->GetVulkanDeviceQueue()->GetFenceHelper();

  if (!command_buf->Submit(begin_semaphores.size(), begin_semaphores.data(),
                           end_semaphores.size(), end_semaphores.data(),
                           is_protected_)) {
    LOG(ERROR) << "Could not submit command buf!";
  }

  fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
      std::move(command_buf));
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](std::unique_ptr<VulkanTextureImage> in_texture,
         std::unique_ptr<VulkanTextureImage> out_texture,
         gpu::VulkanDeviceQueue* device_queue, bool device_lost) {},
      std::move(in_texture), std::move(out_texture)));

  fence_helper->ProcessCleanupTasks();
}

gpu::VulkanDeviceQueue* VulkanOverlayAdaptor::GetVulkanDeviceQueue() {
  return vulkan_device_queue_->GetVulkanDeviceQueue();
}

gpu::VulkanImplementation& VulkanOverlayAdaptor::GetVulkanImplementation() {
  return *vulkan_implementation_;
}

TiledImageFormat VulkanOverlayAdaptor::GetTileFormat() const {
  return tile_format_;
}

}  // namespace media
